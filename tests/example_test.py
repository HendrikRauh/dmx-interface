from pytest_embedded import Dut


def test_basic_expect(redirect, dut: Dut):
    """
    Example test showing the basic usage of expect and expect_exact and
    to verify that pytest is running properly.
    """
    with redirect():
        print("this would be redirected")

    dut.expect("this")
    dut.expect_exact("would")
    dut.expect("[be]{2}")
    dut.expect_exact("redirected")
