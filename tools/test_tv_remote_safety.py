import unittest

from tv_remote_safety import ready


class TvRemoteSafetyTest(unittest.TestCase):
    def test_hid_and_accessibility(self):
        bt = "shim::legacy::hid status:BTIF_HH_DEV_CONNECTED num_devices:1"
        inp = "Device 8: Google TV Remote Keyboard"
        acc = (
            "Enabled services:{{io.github.sds100.keymapper/service}}\n"
            "Bound services:{Service[label=Key Mapper, feedbackType[FEEDBACK_HAPTIC]]}"
        )
        self.assertTrue(ready(bt, inp, acc))
        self.assertFalse(ready(bt.replace("num_devices:1", "num_devices:0"), inp, acc))
        self.assertFalse(ready(bt, "virtual-remote", acc))
        self.assertFalse(ready(bt, inp, acc.replace("Bound services:{Service[label=Key Mapper", "Bound services:{}")))
        self.assertTrue(ready(bt, inp, ""))


if __name__ == "__main__":
    unittest.main()
