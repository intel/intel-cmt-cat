import mock
import pytest

import common
import caps

@mock.patch("common.PQOS_API.is_l3_cat_supported", mock.MagicMock(return_value=True))
@mock.patch("common.PQOS_API.is_mba_supported", mock.MagicMock(return_value=False))
def test_detect_cat_l3():
    assert common.CAT_CAP in caps.detect_supported_caps()


@mock.patch("common.PQOS_API.is_l3_cat_supported", mock.MagicMock(return_value=False))
@mock.patch("common.PQOS_API.is_mba_supported", mock.MagicMock(return_value=False))
def test_detect_cat_l3_negative():
    assert common.CAT_CAP not in caps.detect_supported_caps()


@mock.patch("common.PQOS_API.is_l3_cat_supported", mock.MagicMock(return_value=False))
@mock.patch("common.PQOS_API.is_mba_supported", mock.MagicMock(return_value=True))
def test_detect_mba():
    assert common.MBA_CAP in caps.detect_supported_caps()


@mock.patch("common.PQOS_API.is_l3_cat_supported", mock.MagicMock(return_value=False))
@mock.patch("common.PQOS_API.is_mba_supported", mock.MagicMock(return_value=False))
def test_detect_mba_negative():
    assert common.MBA_CAP not in caps.detect_supported_caps()