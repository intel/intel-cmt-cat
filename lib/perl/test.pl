#!/usr/bin/perl

###############################################################################
# BSD LICENSE
#
# Copyright(c) 2016 Intel Corporation. All rights reserved.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
###############################################################################

# PerlTidy options -bar -ce -pt=2 -sbt=2 -bt=2 -bbt=2 -et=4 -baao -nsfs

use bigint;
use strict;
use warnings;

use pqos;

my $l3ca_cap_p;
my $cpuinfo_p;
my $num_cos;
my $num_ways;
my $num_cores;
my $num_sockets;

sub read_msr {
	my ($cpu_id, $msr) = @_;

	my $cmd;
	if ($^O eq "freebsd") {
		$cmd = sprintf("cpucontrol -m 0x%x /dev/cpuctl%u", $msr, $cpu_id);
	} else {
		$cmd = sprintf("rdmsr -p%u -0 -c 0x%x", $cpu_id, $msr);
	}

	my @result = `$cmd`;

	if (0 != $?) {
		print __LINE__, " $cmd FAILED!\n";
		return;
	}

	if ($^O eq "freebsd") {
		my @result_array = split / /, $result[0];
		return int(($result_array[2] << 32) + $result_array[3]);
	} else {
		return int(hex $result[0]);
	}
}

sub __func__ {
	return (caller(1))[3];
}

sub check_msr_tools {

	if ($^O eq "freebsd") {
		`cpucontrol -m 0xC8F /dev/cpuctl0`;
	} else {
		`rdmsr -p 0 -u 0xC8F`;
	}

	if (-1 == $?) {
		if ($^O eq "freebsd") {
			print __LINE__, " cpucontrol tool not found... ";
		} else {
			print __LINE__, " rdmsr tool not found... ";
		}
		print "please install.\n";
		return -1;
	}

	if (!defined read_msr(0, 0xC8F)) {
		print __LINE__, " unable to read MSR!...\n";
		return -1;
	}

	return 0;
}

# Function to shutdown libpqos
sub shutdown_pqos {
	print "Shutting down pqos lib...\n";
	pqos::pqos_fini();
	$l3ca_cap_p = undef;
	$cpuinfo_p  = undef;
	return;
}

# Function to initialize libpqos
sub init_pqos {
	my $cfg = pqos::pqos_config->new();
	my $ret = -1;

	$cfg->{verbose} = 2;                             # SUPER_VERBOSE
	$cfg->{fd_log}  = fileno(STDOUT);

	if (0 != pqos::pqos_init($cfg)) {
		print __LINE__, " pqos::pqos_init FAILED!\n";
		goto EXIT;
	}

	$l3ca_cap_p = pqos::get_cap_l3ca();
	if (0 == $l3ca_cap_p) {
		print __LINE__, " pqos::get_cap_l3ca FAILED!\n";
		goto EXIT;
	}

	my $l3ca_cap = pqos::l3ca_cap_p_value($l3ca_cap_p);
	$num_cos  = $l3ca_cap->{num_classes};
	$num_ways = $l3ca_cap->{num_ways};

	$cpuinfo_p = pqos::get_cpuinfo();
	if (0 == $cpuinfo_p) {
		print __LINE__, " pqos::get_cpuinfo FAILED!\n";
		goto EXIT;
	}

	$num_cores = pqos::cpuinfo_p_value($cpuinfo_p)->{num_cores};

	(my $result, $num_sockets) = pqos::pqos_cpu_get_num_sockets($cpuinfo_p);
	if (0 != $result) {
		print __LINE__, " pqos::pqos_cpu_get_num_sockets FAILED!\n";
		goto EXIT;
	}

	$ret = 0;

  EXIT:
	return sprintf("%s: %s", __func__, $ret == 0 ? "PASS" : "FAILED!");
}

# Function to generate CoS IDs - for testing purposes only
sub generate_cos {
	my ($cpu_id, $socket_id) = @_;

	if (!defined $num_cos) {
		print __LINE__, " !defined ... FAILED!\n";
		return;
	}

	return ($cpu_id + $socket_id) % $num_cos;
}

# Function to generate CoS's way mask - for testing purposes only
sub generate_ways_mask {
	my ($cos_id, $socket_id) = @_;

	if (!defined $num_ways || !defined $num_cos) {
		print __LINE__, " !defined ... FAILED!\n";
		return;
	}

	my $bits_per_cos = int($num_ways / $num_cos);
	if ($bits_per_cos == 0) {
		$bits_per_cos = 1;
	}

	my $base_mask = (1 << ($bits_per_cos)) - 1;
	my $ways_mask = $base_mask << ($cos_id * $bits_per_cos + $socket_id % 3);
	my $result = $ways_mask & (2**$num_ways - 1);

	if (0 == $result) {
		$result = $base_mask;
	}

	return $result;
}

# Function to get CoS assigned to CPU via MSRs (using rdmsr from msr-tools)
sub get_msr_assoc {
	my ($cpu_id) = @_;
	return read_msr($cpu_id, 0xC8F) >> 32;
}

# Function to get CoS ways mask via MSRs (using rdmsr from msr-tools)
sub get_msr_ways_mask {
	my ($cos_id, $socket_id) = @_;
	my $cpu_id;

	if (!defined $cpuinfo_p || !defined $num_cores) {
		print __LINE__, " !defined ... FAILED!\n";
		return;
	}

	for ($cpu_id = 0; $cpu_id < $num_cores; $cpu_id++) {
		(my $result, my $cpu_socket_id) =
		  pqos::pqos_cpu_get_socketid($cpuinfo_p, $cpu_id);
		if (0 != $result) {
			print __LINE__, " pqos::pqos_cpu_get_socketid FAILED!\n";
			return;
		}

		if ($socket_id == $cpu_socket_id) {
			last;
		}
	}

	return read_msr($cpu_id, int(0xC90 + $cos_id)) & (2**32 - 1);
}

# Function to print current CAT configuration
sub print_cfg {
	my $socket_id;
	my $cos_id;
	my $l3ca = pqos::pqos_l3ca->new();

	if (!defined $cpuinfo_p ||
		!defined $num_cores   ||
		!defined $num_sockets ||
		!defined $num_cos) {
		return -1;
	}

	print "CoS configuration:\n";

	for ($socket_id = 0; $socket_id < $num_sockets; $socket_id++) {
		for ($cos_id = 0; $cos_id < $num_cos; $cos_id++) {
			if (0 != pqos::get_l3ca($l3ca, $socket_id, $cos_id)) {
				print __LINE__, " pqos::get_l3ca FAILED!\n";
				return -1;
			}

			printf("Socket: %d, CoS: %d, CDP: %d",
				$socket_id, $l3ca->{class_id}, $l3ca->{cdp});
			if (int($l3ca->{cdp}) == 1) {
				printf(", data_mask: 0x%x, code_mask: 0x%x",
					$l3ca->{u}->{s}->{data_mask}, $l3ca->{u}->{s}->{code_mask});
			} else {
				printf(", ways_mask: 0x%x", $l3ca->{u}->{ways_mask});
			}

			print "\n";
		}
	}

	print "CoS association:\n";

	for (my $cpu_id = 0; $cpu_id < $num_cores; $cpu_id++) {
		(my $result, $cos_id) = pqos::pqos_alloc_assoc_get($cpu_id);
		if (0 != $result) {
			print __LINE__, " pqos::pqos_alloc_assoc_get FAILED!\n";
			return -1;
		}

		($result, $socket_id) =
		  pqos::pqos_cpu_get_socketid($cpuinfo_p, $cpu_id);
		if (0 != $result) {
			print __LINE__, " pqos::pqos_cpu_get_socketid FAILED!\n";
			return -1;
		}

		print "Socket: ", $socket_id, ", Core: ", $cpu_id, ", CoS: ", $cos_id,
		  "\n";
	}

	return 0;
}

# Function to test CoS association to CPUs
# Association is configured using libpqos API
# Association is verified using libpqos API and also by reading MSRs
sub test_assoc {
	my $cos_id;
	my $gen_cos_id;
	my $socket_id;
	my $result;
	my $ret = -1;

	if (!defined $cpuinfo_p || !defined $num_cores) {
		goto EXIT;
	}

	for (my $cpu_id = 0; $cpu_id < $num_cores; $cpu_id++) {
		($result, $cos_id) = pqos::pqos_alloc_assoc_get($cpu_id);
		if (0 != $result) {
			print __LINE__, " pqos::pqos_alloc_assoc_get FAILED!\n";
			goto EXIT;
		}

		($result, $socket_id) =
		  pqos::pqos_cpu_get_socketid($cpuinfo_p, $cpu_id);
		if (0 != $result) {
			print __LINE__, " pqos::pqos_cpu_get_socketid FAILED!\n";
			goto EXIT;
		}

		$gen_cos_id = generate_cos($cpu_id, $socket_id);
		if (!defined $gen_cos_id) {
			print __LINE__, " generate_cos FAILED!\n";
			goto EXIT;
		}

		if (0 != pqos::pqos_alloc_assoc_set($cpu_id, $gen_cos_id)) {
			print __LINE__, " pqos::pqos_alloc_assoc_set FAILED!\n";
			goto EXIT;
		}
	}

	for (my $cpu_id = 0; $cpu_id < $num_cores; $cpu_id++) {
		($result, $cos_id) = pqos::pqos_alloc_assoc_get($cpu_id);
		if (0 != $result) {
			print __LINE__, " pqos::pqos_alloc_assoc_get FAILED!\n";
			goto EXIT;
		}

		($result, $socket_id) =
		  pqos::pqos_cpu_get_socketid($cpuinfo_p, $cpu_id);
		if (0 != $result) {
			print __LINE__, " pqos::pqos_cpu_get_socketid FAILED!\n";
			goto EXIT;
		}

		$gen_cos_id = generate_cos($cpu_id, $socket_id);
		if (!defined $gen_cos_id) {
			print __LINE__, " generate_cos FAILED!\n";
			goto EXIT;
		}

		if ($cos_id != $gen_cos_id) {
			print __LINE__, ' $cos_id != $gen_cos_id FAILED!', "\n";
			goto EXIT;
		}

		$cos_id = get_msr_assoc($cpu_id);
		if (!defined $cos_id) {
			print __LINE__, " get_msr_assoc FAILED!\n";
			goto EXIT;
		}

		if ($cos_id != $gen_cos_id) {
			print __LINE__, " msr $cos_id != $gen_cos_id FAILED!\n";
			goto EXIT;
		}
	}

	$ret = 0;

  EXIT:
	return sprintf("%s: %s", __func__, $ret == 0 ? "PASS" : "FAILED!");
}

# Function to test CoS ways masks configuration
# CoS is configured using libpqos API
# CoS configuration is verified using libpqos API and also by reading MSRs
sub test_way_masks {
	my $l3ca = pqos::pqos_l3ca->new();
	my $gen_ways_mask;
	my $msr_ways_mask;
	my $ret = -1;

	if (!defined $num_sockets || !defined $num_cos) {
		goto EXIT;
	}

	for (my $socket_id = 0; $socket_id < $num_sockets; $socket_id++) {
		for (my $cos_id = 0; $cos_id < $num_cos; $cos_id++) {
			$gen_ways_mask = generate_ways_mask($cos_id, $socket_id);
			if (!defined $gen_ways_mask) {
				print __LINE__, " generate_ways_mask FAILED!\n";
				goto EXIT;
			}
			$l3ca->{u}->{ways_mask} = $gen_ways_mask;
			$l3ca->{class_id}  = $cos_id;
			$l3ca->{cdp}       = 0;

			if (0 != pqos::pqos_l3ca_set($socket_id, 1, $l3ca)) {
				print __LINE__, " pqos::pqos_l3ca_set FAILED!\n";
				goto EXIT;
			}
		}
	}

	for (my $socket_id = 0; $socket_id < $num_sockets; $socket_id++) {
		for (my $cos_id = 0; $cos_id < $num_cos; $cos_id++) {
			if (0 != pqos::get_l3ca($l3ca, $socket_id, $cos_id)) {
				print __LINE__, " pqos::get_l3ca FAILED!\n";
				goto EXIT;
			}

			$gen_ways_mask = generate_ways_mask($cos_id, $socket_id);
			if (!defined $gen_ways_mask) {
				print __LINE__, " generate_ways_mask FAILED!\n";
				goto EXIT;
			}

			if ($l3ca->{u}->{ways_mask} != $gen_ways_mask) {
				print __LINE__, ' $l3ca->{u}->{ways_mask} != $gen_ways_mask FAILED!',
				  "\n";
				goto EXIT;
			}

			$msr_ways_mask = get_msr_ways_mask($cos_id, $socket_id);
			if (!defined $msr_ways_mask) {
				print __LINE__, " get_msr_ways_mask FAILED!\n";
				goto EXIT;
			}

			if ($msr_ways_mask != $gen_ways_mask) {
				print __LINE__, ' $msr_ways_mask != $gen_ways_mask FAILED!',
				  "\n";
				goto EXIT;
			}
		}
	}

	$ret = 0;

  EXIT:
	return sprintf("%s: %s", __func__, 0 == $ret ? "PASS" : "FAILED!");
}

# Function to reset CAT configuration - for testing purposes only
sub reset_cfg {
	my $l3ca = pqos::pqos_l3ca->new();
	my $cos_id;
	my $ret = -1;

	if (!defined $num_ways ||
		!defined $num_cores   ||
		!defined $num_sockets ||
		!defined $num_cos) {
		goto EXIT;
	}

	for (my $cpu_id = 0; $cpu_id < $num_cores; $cpu_id++) {
		if (0 != pqos::pqos_alloc_assoc_set($cpu_id, 0)) {
			print __LINE__, " pqos::pqos_alloc_assoc_set FAILED!\n";
			goto EXIT;
		}
	}

	for (my $socket_id = 0; $socket_id < $num_sockets; $socket_id++) {
		for ($cos_id = 0; $cos_id < $num_cos; $cos_id++) {

			if (0 != pqos::get_l3ca($l3ca, $socket_id, $cos_id)) {
				print __LINE__, " pqos::get_l3ca FAILED!\n";
				goto EXIT;
			}

			$l3ca->{cdp}       = 0;
			$l3ca->{u}->{ways_mask} = (1 << $num_ways) - 1;

			if (0 != pqos::pqos_l3ca_set($socket_id, 1, $l3ca)) {
				print __LINE__, " pqos::pqos_l3ca_set FAILED!\n";
				goto EXIT;
			}
		}
	}

	$ret = 0;

  EXIT:
	return sprintf("%s: %s", __func__, $ret == 0 ? "PASS" : "FAILED!");
}

(0 == check_msr_tools) or die("MSR reading tool issue...\n");

printf("%d %s\n", __LINE__, init_pqos());
printf("%d %s\n", __LINE__, test_assoc());
printf("%d %s\n", __LINE__, test_way_masks());

print_cfg;

printf("%d %s\n", __LINE__, reset_cfg());

shutdown_pqos();
