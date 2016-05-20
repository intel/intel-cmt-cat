#!/usr/bin/perl
###############################################################################
#
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

=head1 NAME

    cat-agentx.pl - Net-SNMP AgentX subagent

=head1 DESCRIPTION

    This is a Net-SNMP AgentX subagent written in Perl to demonstrate
    the use of the PQoS library Perl wrapper API. This subagent allows to
    read and change CAT configuration via SNMP.

    Supported OIDs:
    COS configuration OID: SNMPv2-SMI::enterprises.343.3.0.x.y.z
    x - socket id
    y - cos id
    z - field (cdp (RO): 1, ways_mask: 2, data_mask: 3, code_mask: 4)

    value:
     - z = 1, current CDP setting, Read-Only
     - z = 2, ways_mask configuration
     - z = 3, data_mask configuration (if CDP == 1)
     - z = 4, code_mask configuration (if CDP == 1)

    CPU-COS association OID: SNMPv2-SMI::enterprises.343.3.1.x.y
    x - socket id
    y - cpu id
    value - COS ID

=cut

use bigint;
use strict;
use warnings;
use Readonly;

use NetSNMP::agent (':all');
use NetSNMP::ASN   (':all');
use NetSNMP::OID;

use pqos;

my %data_cos        = ();
my @data_cos_keys   = ();
my %data_assoc      = ();
my @data_assoc_keys = ();
my $data_timestamp  = 0;

Readonly my $DATA_TIMEOUT => 1;

Readonly::Scalar my $OID_NUM_ENTER        => ".1.3.6.1.4.1";
Readonly::Scalar my $OID_NUM_INTEL        => "$OID_NUM_ENTER.343";
Readonly::Scalar my $OID_NUM_EXPERIMENTAL => "$OID_NUM_INTEL.3";

Readonly::Scalar my $OID_NUM_PQOS_COS => "$OID_NUM_EXPERIMENTAL.0";
Readonly my $COS_CDP_ID               => 0;
Readonly my $COS_WAYS_MASK_ID         => 1;
Readonly my $COS_DATA_MASK_ID         => 2;
Readonly my $COS_CODE_MASK_ID         => 3;

Readonly::Scalar my $OID_NUM_PQOS_ASSOC => "$OID_NUM_EXPERIMENTAL.1";

my $oid_pqos_cos;
my $oid_pqos_assoc;

my $l3ca_cap_p;
my $cpuinfo_p;

my $num_cos;
my $num_ways;
my $num_cores;
my $num_sockets;

my $active = 1;

=item shutdown_agent()

Shutdowns AgentX agent

=cut

sub shutdown_agent {
	print "Shutting down pqos lib...\n";
	pqos::pqos_fini();
	$active = 0;
	return;
}

=item pqos_init()

 Returns: 0 on success, -1 otherwise

Initializes PQoS library

=cut

sub pqos_init {
	my $cfg = pqos::pqos_config->new();

	$cfg->{verbose} = 2;                             # SUPER_VERBOSE
	$cfg->{fd_log}  = fileno(STDOUT);
	$cfg->{cdp_cfg} = $pqos::PQOS_REQUIRE_CDP_ANY;

	if (0 != pqos::pqos_init($cfg)) {
		print __LINE__, " pqos::pqos_init FAILED!\n";
		return -1;
	}

	$l3ca_cap_p = pqos::get_cap_l3ca();
	if (0 == $l3ca_cap_p) {
		print __LINE__, " pqos::get_cap_l3ca FAILED!\n";
		return -1;
	}

	my $l3ca_cap = pqos::l3ca_cap_p_value($l3ca_cap_p);
	$num_cos  = $l3ca_cap->{num_classes};
	$num_ways = $l3ca_cap->{num_ways};

	$cpuinfo_p = pqos::get_cpuinfo();
	if (0 == $cpuinfo_p) {
		print __LINE__, " pqos::get_cpuinfo FAILED!\n";
		return -1;
	}

	$num_cores = pqos::cpuinfo_p_value($cpuinfo_p)->{num_cores};

	(my $result, $num_sockets) = pqos::pqos_cpu_get_num_sockets($cpuinfo_p);
	if (0 != $result) {
		print __LINE__, " pqos::pqos_cpu_get_num_sockets FAILED!\n";
		return -1;
	}

	return 0;
}

=item data_update()

Reads current CAT configuration
and updates local hashes with OIDs and values

=cut

sub data_update {
	my $ret = 0;
	my $now = time;
	if (($now - $data_timestamp) < $DATA_TIMEOUT) {
		return;
	}
	$data_timestamp = $now;

	%data_assoc = ();
	for (my $cpu_id = 0; $cpu_id < $num_cores; $cpu_id++) {
		($ret, my $cos_id) = pqos::pqos_alloc_assoc_get($cpu_id);
		if (0 != $ret) {
			last;
		}

		($ret, my $socket_id) =
		  pqos::pqos_cpu_get_socketid($cpuinfo_p, $cpu_id);
		if (0 != $ret) {
			last;
		}

		my $oid = NetSNMP::OID->new("$OID_NUM_PQOS_ASSOC.$socket_id.$cpu_id");
		$data_assoc{$oid} = $cos_id;
	}

	@data_assoc_keys =
	  sort {NetSNMP::OID->new($a) <=> NetSNMP::OID->new($b)}
	  keys %data_assoc;

	%data_cos = ();
	my $l3ca = pqos::pqos_l3ca->new();

	for (my $socket_id = 0; $socket_id < $num_sockets; $socket_id++) {
		for (my $cos_id = 0; $cos_id < $num_cos; $cos_id++) {
			if (0 != pqos::get_l3ca($l3ca, $socket_id, $cos_id)) {
				last;
			}

			my $oid_num = "$OID_NUM_PQOS_COS.$socket_id.$l3ca->{class_id}";
			$data_cos{new NetSNMP::OID("$oid_num.$COS_CDP_ID")} =
			  int($l3ca->{cdp});

			if (1 == int($l3ca->{cdp})) {
				$data_cos{new NetSNMP::OID("$oid_num.$COS_DATA_MASK_ID")} =
				  int($l3ca->{data_mask});
				$data_cos{new NetSNMP::OID("$oid_num.$COS_CODE_MASK_ID")} =
				  int($l3ca->{code_mask});
			} else {
				$data_cos{new NetSNMP::OID("$oid_num.$COS_WAYS_MASK_ID")} =
				  int($l3ca->{ways_mask});
			}
		}
	}

	@data_cos_keys =
	  sort {NetSNMP::OID->new($a) <=> NetSNMP::OID->new($b)} keys %data_cos;

	return;
}

=item is_whole_number()

 Arguments:
    $number: number to be tested

 Returns: true if number is non-negative integer

Test if number is non-negative integer.

=cut

sub is_whole_number {
	my ($number) = @_;
	return $number =~ /^\d+$/;
}

=item bits_count()

 Arguments:
    $bitmask: bitmask to be checked for set bits

 Returns: number of set bits

Counts number of set bits in a bitmask.

=cut

sub bits_count {
	my ($bitmask) = @_;
	my $count = 0;

	for (; $bitmask != 0; $count++) {
		$bitmask &= $bitmask - 1;
	}

	return $count;
}

=item is_contiguous()

 Arguments:
    $bitmask: bitmask to be tested

 Returns: true if bitmask is contiguous

Test if bitmask is contiguous.

=cut

sub is_contiguous {
	my ($bitmask) = @_;
	Readonly my $MAX_IDX => 64;
	my $j = 0;

	if ($bitmask && (2**$MAX_IDX - 1) == 0) {
		return 0;
	}

	for (my $i = 0; $i < $MAX_IDX; $i++) {
		if (((1 << $i) & $bitmask) != 0) {
			$j++;
		} elsif ($j > 0) {
			last;
		}
	}

	if (bits_count($bitmask) != $j) {
		return 0;
	}

	return 1;
}

=item handle_assoc()

 Arguments:
    $request, $request_info: request related data

Handle $OID_NUM_PQOS_ASSOC OID requests

=cut

sub handle_assoc {
	my ($request, $request_info) = @_;
	my $oid  = $request->getOID();
	my $mode = $request_info->getMode();

	if (MODE_GET == $mode) {
		if (exists $data_assoc{$oid}) {
			$request->setValue(ASN_UNSIGNED, $data_assoc{$oid});
		}

		return;
	}

	if (MODE_GETNEXT == $mode) {
		foreach (@data_assoc_keys) {
			$_ = NetSNMP::OID->new($_);
			if ($oid < $_) {
				$request->setOID($_);
				$request->setValue(ASN_UNSIGNED, $data_assoc{$_});
				last;
			}
		}

		return;
	}

	if (MODE_SET_RESERVE1 == $mode) {
		my $value = $request->getValue();
		if (!exists $data_assoc{$oid}) {
			$request->setError($request_info, SNMP_ERR_NOSUCHNAME);
		} elsif (!is_whole_number($value)) {
			$request->setError($request_info, SNMP_ERR_WRONGTYPE);
		} elsif ($value < 0 || $value >= $num_cos) {
			$request->setError($request_info, SNMP_ERR_WRONGVALUE);
		}

		return;
	}

	if (MODE_SET_ACTION == $mode) {
		my $cpu_id = ($oid->to_array())[$oid->length - 1];
		my $cos_id = $request->getValue();
		if (0 != pqos::pqos_alloc_assoc_set($cpu_id, $cos_id)) {
			$request->setError($request_info, SNMP_ERR_GENERR);
		}
	}

	return;
}

=item handle_cos()

 Arguments:
    $request, $request_info: request related data

Handle $OID_NUM_PQOS_COS OID requests

=cut

sub handle_cos {
	my ($request, $request_info) = @_;
	my $oid  = $request->getOID();
	my $mode = $request_info->getMode();

	if (MODE_GET == $mode) {
		if (exists $data_cos{$oid}) {
			$request->setValue(ASN_COUNTER64, $data_cos{$oid});
		}
		return;
	}

	if (MODE_GETNEXT == $mode) {
		foreach (@data_cos_keys) {
			$_ = NetSNMP::OID->new($_);
			if ($oid < $_) {
				$request->setOID($_);
				$request->setValue(ASN_COUNTER64, $data_cos{$_});
				last;
			}
		}
		return;
	}

	if (MODE_SET_RESERVE1 == $mode) {
		my $value = $request->getValue();
		if (!exists $data_cos{$oid}) {
			$request->setError($request_info, SNMP_ERR_NOSUCHNAME);
			return;
		}

		my $field_id = ($oid->to_array())[$oid->length - 1];

		if ($field_id != $COS_WAYS_MASK_ID &&
			$field_id != $COS_DATA_MASK_ID &&
			$field_id != $COS_CODE_MASK_ID) {
			$request->setError($request_info, SNMP_ERR_READONLY);
			return;
		}

		if (!is_whole_number($value)) {
			$request->setError($request_info, SNMP_ERR_WRONGTYPE);
			return;
		}

		if ($value <= 0 || $value >= (2**$num_ways)) {
			$request->setError($request_info, SNMP_ERR_WRONGVALUE);
			return;
		}

		if (!is_contiguous($value)) {
			$request->setError($request_info, SNMP_ERR_WRONGVALUE);
			return;
		}

		return;
	}

	if (MODE_SET_ACTION == $mode) {
		my $value     = $request->getValue();
		my $socket_id = ($oid->to_array())[$oid->length - 3];
		my $cos_id    = ($oid->to_array())[$oid->length - 2];
		my $field_id  = ($oid->to_array())[$oid->length - 1];
		my $l3ca      = pqos::pqos_l3ca->new();

		if (0 != pqos::get_l3ca($l3ca, $socket_id, $cos_id)) {
			$request->setError($request_info, SNMP_ERR_GENERR);
			return;
		}

		if ($field_id == $COS_WAYS_MASK_ID) {
			$l3ca->{ways_mask} = $value;
		} elsif ($field_id == $COS_DATA_MASK_ID) {
			$l3ca->{data_mask} = $value;
		} elsif ($field_id == $COS_CODE_MASK_ID) {
			$l3ca->{code_mask} = $value;
		}

		if (0 != pqos::pqos_l3ca_set($socket_id, 1, $l3ca)) {
			$request->setError($request_info, SNMP_ERR_GENERR);
		}
	}

	return;
}

=item handle_snmp_req()

 Arguments:
    $handler, $registration_info, ... : request related data

Subroutine registered as a request handler, calls appropriate handlers

=cut

sub handle_snmp_req {
	my ($handler, $registration_info, $request_info, $requests) = @_;

	data_update;

	my $root_oid = $registration_info->getRootOID();

	for (my $request = $requests; $request; $request = $request->next()) {
		if (MODE_SET_COMMIT == $request_info->getMode()) {
			$data_timestamp = 0;
		} elsif ($root_oid == $oid_pqos_cos) {
			handle_cos($request, $request_info);
		} elsif ($root_oid == $oid_pqos_assoc) {
			handle_assoc($request, $request_info);
		}
	}

	return;
}

if (0 == pqos_init) {
	local $SIG{'INT'}  = \&shutdown_agent;
	local $SIG{'QUIT'} = \&shutdown_agent;

	my $agent = NetSNMP::agent->new('Name' => "pqos", 'AgentX' => 1);

	$oid_pqos_cos   = NetSNMP::OID->new($OID_NUM_PQOS_COS);
	$oid_pqos_assoc = NetSNMP::OID->new($OID_NUM_PQOS_ASSOC);

	$agent->register("pqos_cos_cfg", $oid_pqos_cos, \&handle_snmp_req) or
	  die "registration of pqos_cos_cfg handler failed!\n";

	$agent->register("pqos_cos_assoc", $oid_pqos_assoc, \&handle_snmp_req) or
	  die "registration of pqos_cos_assoc handler failed!\n";

	while ($active) {
		$agent->agent_check_and_process(1);
	}

	$agent->shutdown();
}
