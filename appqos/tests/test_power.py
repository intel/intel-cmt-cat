################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2020 Intel Corporation. All rights reserved.
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
################################################################################

import mock
import pytest

import power
import power_common
import sstbf

class TestRestPowerProfiles:


    def test_is_sstcp_enabled(self):
        class SYS:
            def __init__(self, enabled):
                self.epp_enabled = enabled
                self.sst_bf_enabled = enabled

        with mock.patch("pwr.get_system", return_value=SYS(True)) as mock_get_system:
            assert power.is_sstcp_enabled()
            mock_get_system.assert_called_once()

        with mock.patch("pwr.get_system", return_value=SYS(False)) as mock_get_system:
            assert not power.is_sstcp_enabled()
            mock_get_system.assert_called_once()

        with mock.patch("pwr.get_system", return_value=None) as mock_get_system:
            assert not power.is_sstcp_enabled()
            mock_get_system.assert_called_once()

        with mock.patch("pwr.get_system", side_effect = IOError('Test')) as mock_get_system:
            assert not power.is_sstcp_enabled()
            mock_get_system.assert_called_once()


    def test__get_pwr_cpus(self):
        class CPU:
            def __init__(self, core_list):
                self.core_list=core_list

        CPUS_LIST = [CPU([]), CPU([])]
        with mock.patch("pwr.get_cpus", return_value=CPUS_LIST) as mock_get_cpus:
            assert power_common.get_pwr_cpus() == CPUS_LIST
            mock_get_cpus.assert_called_once()

        with mock.patch("pwr.get_cpus", return_value=[]) as mock_get_cpus:
            assert not power_common.get_pwr_cpus()
            mock_get_cpus.assert_called_once()

        with mock.patch("pwr.get_cpus", return_value=None) as mock_get_cpus:
            assert not power_common.get_pwr_cpus()
            mock_get_cpus.assert_called_once()

        with mock.patch("pwr.get_cpus", side_effect = IOError('Test')) as mock_get_cpus:
            assert not power_common.get_pwr_cpus()
            mock_get_cpus.assert_called_once()

        with mock.patch("pwr.get_cpus", side_effect = ValueError('Test')) as mock_get_cpus:
            assert not power_common.get_pwr_cpus()
            mock_get_cpus.assert_called_once()


    def test__get_pwr_cores(self):
        class CORE:
            def __init__(self, id):
                self.core_id=id

        CORES_LIST = [CORE(0), CORE(1)]

        with mock.patch("pwr.get_cores", return_value=CORES_LIST) as mock_get_cores:
            assert power_common.get_pwr_cores() == CORES_LIST
            mock_get_cores.assert_called_once()

        with mock.patch("pwr.get_cores", return_value=[]) as mock_get_cores:
            assert not power_common.get_pwr_cores()
            mock_get_cores.assert_called_once()

        with mock.patch("pwr.get_cores", return_value=None) as mock_get_cores:
            assert not power_common.get_pwr_cores()
            mock_get_cores.assert_called_once()

        with mock.patch("pwr.get_cores", side_effect = IOError('Test')) as mock_get_cores:
            assert not power_common.get_pwr_cores()
            mock_get_cores.assert_called_once()

        with mock.patch("pwr.get_cores", side_effect = ValueError('Test')) as mock_get_cores:
            assert not power_common.get_pwr_cores()
            mock_get_cores.assert_called_once()


    def test__set_freqs_epp(self):
        class CORE:
            def __init__(self, id):
                self.core_id=id
                self.commit = mock.MagicMock()

        CORES_LIST = [CORE(0), CORE(1), CORE(2), CORE(3), CORE(4)]

        assert -1 == power._set_freqs_epp(cores=None, min_freq=1000)

        assert -1 == power._set_freqs_epp(cores=[], min_freq=1000)

        assert -1 == power._set_freqs_epp(cores=[1,2,3], min_freq=None, max_freq=None, epp=None)

        for ret_val in [[], None]:
            with mock.patch("power_common.get_pwr_cores", return_value=ret_val) as mock_get_cores:
                assert -1 == power._set_freqs_epp(cores=[1,2,3], min_freq=1000, max_freq=2300, epp="performance")
                mock_get_cores.assert_called_once()

        with mock.patch("power_common.get_pwr_cores", return_value=CORES_LIST) as mock_get_cores:
            assert 0 == power._set_freqs_epp(cores=[1,2,3], min_freq=1000, max_freq=2300, epp="performance")
            mock_get_cores.assert_called_once()

            for core in CORES_LIST:
                if core.core_id in [1, 2, 3]:
                    assert core.min_freq == 1000
                    assert core.max_freq == 2300
                    assert core.epp == "performance"
                    core.commit.assert_called_once()
                else:
                    assert not hasattr(core, 'min_freq')
                    assert not hasattr(core, 'max_freq')
                    assert not hasattr(core, 'epp')
                    core.commit.assert_not_called()


    def test_reset(self):
        class CORE:
            def __init__(self, id):
                self.core_id=id
                self.commit = mock.MagicMock()

        CORES_LIST = [CORE(0), CORE(1), CORE(2), CORE(3), CORE(4)]

        for val in [[], None]:
            assert -1 == power.reset(val)

        with mock.patch("power_common.get_pwr_cores", return_value=CORES_LIST) as mock_get_cores:
            assert 0 == power.reset([0, 1, 2])
            mock_get_cores.assert_called_once()

            for core in CORES_LIST:
                if core.core_id in [0, 1, 2]:
                    core.commit.assert_called_once_with("default")
                else:
                    core.commit.assert_not_called()


    def test__get_lowest_freq(self):
        class CPU:
            def __init__(self, freq):
                self.lowest_freq = freq

        for ret_val in [[], None]:
            with mock.patch("power_common.get_pwr_cpus", return_value = ret_val) as mock_get_cpus:
                assert None == power_common.get_pwr_lowest_freq()
                mock_get_cpus.assert_called_once()

        for freq in [1000, 1500, 2000]:
            with mock.patch("power_common.get_pwr_cpus", return_value=[CPU(freq)]) as mock_get_cpus:
                assert freq == power_common.get_pwr_lowest_freq()
                mock_get_cpus.assert_called_once()


    def test__get_base_freq(self):
        class CPU:
            def __init__(self, freq):
                self.base_freq = freq

        for ret_val in [[], None]:
            with mock.patch("power_common.get_pwr_cpus", return_value = ret_val) as mock_get_cpus:
                assert None == power_common.get_pwr_base_freq()
                mock_get_cpus.assert_called_once()

        for freq in [2000, 2500, 3000]:
            with mock.patch("power_common.get_pwr_cpus", return_value=[CPU(freq)]) as mock_get_cpus:
                assert freq == power_common.get_pwr_base_freq()
                mock_get_cpus.assert_called_once()


    def test__get_highest_freq(self):
        class CPU:
            def __init__(self, freq):
                self.highest_freq = freq

        for ret_val in [[], None]:
            with mock.patch("power_common.get_pwr_cpus", return_value = ret_val) as mock_get_cpus:
                assert None == power_common.get_pwr_highest_freq()
                mock_get_cpus.assert_called_once()

        for freq in [3000, 3500, 4000]:
            with mock.patch("power_common.get_pwr_cpus", return_value=[CPU(freq)]) as mock_get_cpus:
                assert freq == power_common.get_pwr_highest_freq()
                mock_get_cpus.assert_called_once()


    def test__is_min_freq_valid(self):

        for ret_val in [[], None]:
            with mock.patch("power_common.get_pwr_lowest_freq", return_value = ret_val) as mock_get:
                assert None == power._is_min_freq_valid(1000)
                mock_get.assert_called_once()

        for freq in [500, 1000, 1499]:
            with mock.patch("power_common.get_pwr_lowest_freq", return_value = 1500) as mock_get:
                assert False == power._is_min_freq_valid(freq)
                mock_get.assert_called_once()

        for freq in [1000, 1500, 2000]:
            with mock.patch("power_common.get_pwr_lowest_freq", return_value = 1000) as mock_get:
                assert True == power._is_min_freq_valid(freq)
                mock_get.assert_called_once()


    def test__is_max_freq_valid(self):

        for ret_val in [[], None]:
            with mock.patch("power_common.get_pwr_highest_freq", return_value = ret_val) as mock_get:
                assert None == power._is_max_freq_valid(1000)
                mock_get.assert_called_once()

        for freq in [3501, 3750, 4000]:
            with mock.patch("power_common.get_pwr_highest_freq", return_value = 3500) as mock_get:
                assert False == power._is_max_freq_valid(freq)
                mock_get.assert_called_once()

        for freq in [2000, 3000, 3500]:
            with mock.patch("power_common.get_pwr_highest_freq", return_value = 3500) as mock_get:
                assert True == power._is_max_freq_valid(freq)
                mock_get.assert_called_once()


    def test__is_epp_valid(self):

        for epp in [None, "", 1000, "TEST", "inValid_EPP", []]:
            assert False == power._is_epp_valid(epp)

        for epp in power.VALID_EPP + [power.DEFAULT_EPP]:
            assert True == power._is_epp_valid(epp)


    def test__do_admission_control_check(self):

        for sstbf in [True, False]:
            result = not sstbf

            with mock.patch('sstbf.is_sstbf_configured', return_value = sstbf):
                assert result == power._do_admission_control_check()


    def test_validate_power_profiles(self):

        data = {
            "power_profiles": [
                {
                    "id": 0,
                    "min_freq": 1500,
                    "max_freq": 2500,
                    "epp": "performance",
                    "name": "default"
                },
                {
                    "id": 0,
                    "min_freq": 1000,
                    "max_freq": 1000,
                    "epp": "power",
                    "name": "low_priority"
                }
            ]
        }

        with mock.patch('power._is_max_freq_valid', return_value = True) as mock_is_max,\
            mock.patch('power._is_min_freq_valid', return_value = True) as mock_is_min,\
            mock.patch('power._is_epp_valid', return_value = True) as mock_is_epp,\
            mock.patch('power._do_admission_control_check', return_value = True),\
            mock.patch('power._admission_control_check', return_value = True) as mock_admission_control_check:

            with pytest.raises(ValueError, match="Power Profile 0, multiple profiles with same id."):
                power.validate_power_profiles(data, True)
            mock_admission_control_check.assert_not_called()

        # fix profile ID issue
        data['power_profiles'][-1]['id'] = 1

        with mock.patch('power._is_max_freq_valid', return_value = True) as mock_is_max,\
            mock.patch('power._is_min_freq_valid', return_value = True) as mock_is_min,\
            mock.patch('power._is_epp_valid', return_value = True) as mock_is_epp,\
            mock.patch('power._do_admission_control_check', return_value = True),\
            mock.patch('power._admission_control_check', return_value = True) as mock__admission_control_check:

            power.validate_power_profiles(data, True)

            mock_is_max.assert_any_call(2500)
            mock_is_max.assert_any_call(1000)

            mock_is_min.assert_any_call(1500)
            mock_is_min.assert_any_call(1000)

            mock_is_epp.assert_any_call("performance")
            mock_is_epp.assert_any_call("power")

            mock__admission_control_check.assert_called_once()


        with mock.patch('power._is_max_freq_valid', return_value = True) as mock_is_max,\
            mock.patch('power._is_min_freq_valid', return_value = True) as mock_is_min,\
            mock.patch('power._is_epp_valid', return_value = True) as mock_is_epp,\
            mock.patch('power._do_admission_control_check', return_value = False),\
            mock.patch('power._admission_control_check') as mock__admission_control_check:

            power.validate_power_profiles(data, True)
            mock__admission_control_check.assert_not_called()


        with mock.patch('power._is_max_freq_valid', return_value = True) as mock_is_max,\
            mock.patch('power._is_min_freq_valid', return_value = True) as mock_is_min,\
            mock.patch('power._is_epp_valid', return_value = True) as mock_is_epp,\
            mock.patch('power._do_admission_control_check', return_value = True),\
            mock.patch('power._admission_control_check') as mock__admission_control_check:

            power.validate_power_profiles(data, False)
            mock__admission_control_check.assert_not_called()


        sys = mock.MagicMock()
        sys.request_config = mock.MagicMock(return_value=True)
        sys.refresh_all = mock.MagicMock()

        cores = []
        for id in range(0,3):
            core = mock.MagicMock()
            core.core_id = id
            cores.append(core)
        data.update({
                    "pools": [
                    {
                        "id": 0,
                        "cores": [0,1,2,3],
                        "power_profile": 0
                    },
                    {
                        "id": 1,
                        "cores": [4,5,6,7]
                    }]
        })
        with mock.patch('power._is_max_freq_valid', return_value = True) as mock_is_max,\
            mock.patch('power._is_min_freq_valid', return_value = True) as mock_is_min,\
            mock.patch('power._is_epp_valid', return_value = True) as mock_is_epp,\
            mock.patch('power._do_admission_control_check', return_value = True),\
            mock.patch('power_common.get_pwr_cores', return_value=cores) as mock_get_cores,\
            mock.patch('power_common.get_pwr_sys', return_value=sys) as mock_get_sys:

            power.validate_power_profiles(data, True)
            sys.request_config.assert_called_once()
            sys.refresh_all.assert_called_once()

            for core in cores:
                assert hasattr(core, "min_freq")
                assert hasattr(core, "max_freq")
                assert hasattr(core, "epp")


        sys.request_config = mock.MagicMock(return_value=False)
        sys.refresh_all.reset_mock()
        with mock.patch('power._is_max_freq_valid', return_value = True) as mock_is_max,\
            mock.patch('power._is_min_freq_valid', return_value = True) as mock_is_min,\
            mock.patch('power._is_epp_valid', return_value = True) as mock_is_epp,\
            mock.patch('power._do_admission_control_check', return_value = True),\
            mock.patch('power_common.get_pwr_cores', return_value=cores) as mock_get_cores,\
            mock.patch('power_common.get_pwr_sys', return_value=sys) as mock_get_sys:

            with pytest.raises(power.AdmissionControlError, match="Power Profiles configuration would cause CPU to be oversubscribed."):
                power.validate_power_profiles(data, True)
            sys.request_config.assert_called_once()
            sys.refresh_all.assert_called_once()


        with mock.patch('power._is_max_freq_valid',  return_value = False ) as mock_is_max,\
            mock.patch('power._is_min_freq_valid',  return_value = True) as mock_is_min,\
            mock.patch('power._is_epp_valid', return_value = True) as mock_is_epp,\
            mock.patch('power._do_admission_control_check', return_value = True),\
            mock.patch('power._admission_control_check', return_value = True) as mock__admission_control_check:

            with pytest.raises(ValueError, match="Power Profile 0, Invalid max. freq 2500."):
                power.validate_power_profiles(data, True)
            mock__admission_control_check.assert_not_called()


        with mock.patch('power._is_max_freq_valid',  return_value = True ) as mock_is_max,\
            mock.patch('power._is_min_freq_valid',  return_value = False) as mock_is_min,\
            mock.patch('power._is_epp_valid', return_value = True) as mock_is_epp,\
            mock.patch('power._do_admission_control_check', return_value = True),\
            mock.patch('power._admission_control_check', return_value = True) as mock__admission_control_check:

            with pytest.raises(ValueError, match="Power Profile 0, Invalid min. freq 1500."):
                power.validate_power_profiles(data, True)
            mock__admission_control_check.assert_not_called()


        with mock.patch('power._is_max_freq_valid',  return_value = True ) as mock_is_max,\
            mock.patch('power._is_min_freq_valid',  return_value = True) as mock_is_min,\
            mock.patch('power._is_epp_valid', return_value = False) as mock_is_epp,\
            mock.patch('power._do_admission_control_check', return_value = True),\
            mock.patch('power._admission_control_check', return_value = True) as mock__admission_control_check:

            with pytest.raises(ValueError, match="Power Profile 0, Invalid EPP value performance."):
                power.validate_power_profiles(data, True)
            mock__admission_control_check.assert_not_called()


        # set invalid min. freq, higher than max. freq
        data['power_profiles'][-1]['min_freq'] = data['power_profiles'][-1]['max_freq'] + 1

        with mock.patch('power._is_max_freq_valid',  return_value = True) as mock_is_max,\
            mock.patch('power._is_min_freq_valid',  return_value = True) as mock_is_min,\
            mock.patch('power._is_epp_valid', return_value = True) as mock_is_epp,\
            mock.patch('power._do_admission_control_check', return_value = True),\
            mock.patch('power._admission_control_check', return_value = True) as mock__admission_control_check:

            with pytest.raises(ValueError, match="Power Profile 1, Invalid freqs! min. freq is higher than max. freq."):
                power.validate_power_profiles(data, True)
            mock__admission_control_check.assert_not_called()


    def test_configure_power(self):

        pool_ids = [0, 1]
        profile_ids = [5, 6]
        pool_to_cores = {0: [2,3], 1: [4, 5]}
        pool_to_profiles = {0: 5, 1: 6}
        profiles = {5: {"min_freq": 1500, "max_freq": 2500, "epp": "performance", "id": 5},
            6 : {"min_freq": 1000, "max_freq": 1200, "epp": "power", "id": 6}}

        def get_pool_attr(attr, pool_id):
            if attr == 'id':
                if pool_id is None:
                    return pool_ids
                else:
                    return None
            elif attr == 'cores':
                return pool_to_cores[pool_id]
            elif attr == 'power_profile':
                if pool_id is None:
                    return profile_ids
                else:
                    return pool_to_profiles[pool_id]

            return None

        def get_power_profile(id):
            return profiles[id]

        # ERROR POWER: No Pools configured...
        power.PREV_PROFILES.clear()
        with mock.patch('common.CONFIG_STORE.get_pool_attr', return_value=None) as mock_get_pool_attr,\
            mock.patch('common.CONFIG_STORE.get_power_profile') as mock_get_power_profile,\
            mock.patch('power.reset') as mock_reset,\
            mock.patch('power._set_freqs_epp') as mock_set_freqs_epp:

            assert -1 == power.configure_power()
            mock_get_pool_attr.assert_called_once_with('id', None)
            mock_get_power_profile.assert_not_called()
            mock_reset.assert_not_called()
            mock_set_freqs_epp.assert_not_called()

        # ERROR POWER: Profile 5 does not exist!
        power.PREV_PROFILES.clear()
        with mock.patch('common.CONFIG_STORE.get_pool_attr', new=get_pool_attr),\
            mock.patch('common.CONFIG_STORE.get_power_profile', return_value=None) as mock_get_power_profile,\
            mock.patch('power.reset') as mock_reset,\
            mock.patch('power._set_freqs_epp') as mock_set_freqs_epp:

            assert -1 == power.configure_power()
            mock_get_power_profile.assert_called_once_with(5)
            mock_reset.assert_not_called()
            mock_set_freqs_epp.assert_not_called()

        # All OK!
        power.PREV_PROFILES.clear()
        with mock.patch('common.CONFIG_STORE.get_pool_attr', new=get_pool_attr),\
            mock.patch('common.CONFIG_STORE.get_power_profile', new=get_power_profile),\
            mock.patch('power.reset') as mock_reset,\
            mock.patch('power._set_freqs_epp') as mock_set_freqs_epp:

            assert 0 == power.configure_power()
            mock_reset.assert_not_called()
            mock_set_freqs_epp.assert_any_call(pool_to_cores[0], profiles[5]["min_freq"], profiles[5]["max_freq"], profiles[5]["epp"])
            mock_set_freqs_epp.assert_any_call(pool_to_cores[1], profiles[6]["min_freq"], profiles[6]["max_freq"], profiles[6]["epp"])

        # POWER: Skipping Pool 0, no cores assigned
        power.PREV_PROFILES.clear()
        pool_to_cores[0] = []
        with mock.patch('common.CONFIG_STORE.get_pool_attr', new=get_pool_attr),\
            mock.patch('common.CONFIG_STORE.get_power_profile', new=get_power_profile),\
            mock.patch('power.reset') as mock_reset,\
            mock.patch('power._set_freqs_epp') as mock_set_freqs_epp:

            assert 0 == power.configure_power()
            mock_reset.assert_not_called()
            mock_set_freqs_epp.assert_called_once_with(pool_to_cores[1], profiles[6]["min_freq"], profiles[6]["max_freq"], profiles[6]["epp"])

        # POWER: Pool 1, no power profile assigned. Resetting to defaults.
        power.PREV_PROFILES.clear()
        pool_to_profiles[1] = None
        with mock.patch('common.CONFIG_STORE.get_pool_attr', new=get_pool_attr),\
            mock.patch('common.CONFIG_STORE.get_power_profile', new=get_power_profile),\
            mock.patch('power.reset') as mock_reset,\
            mock.patch('power._set_freqs_epp') as mock_set_freqs_epp:

            assert 0 == power.configure_power()
            mock_reset.assert_called_once_with(pool_to_cores[1])
            mock_set_freqs_epp.assert_not_called()
