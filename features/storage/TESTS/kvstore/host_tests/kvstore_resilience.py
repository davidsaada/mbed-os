"""
mbed SDK
Copyright (c) 2018-2018 ARM Limited
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

from __future__ import print_function

from mbed_host_tests import BaseHostTest
from time import sleep
import random


class KVStoreResilienceTest(BaseHostTest):
    """This test resets the board a few times, in order to test all KVStore classes.
    """

    """Number of times to reset the device in this test"""
    RESET_COUNT = 6
    RESET_DELAY_BASE = 4.0
    VALUE_PLACEHOLDER = "0"

    def setup(self):
        """Register callbacks required for the test"""
        self._error = False
        generator = self.kvstore_resilience_test()
        generator.next()

        def run_gen(key, value, time):
            """Run the generator, and fail testing if the iterator stops"""
            if self._error:
                return
            try:
                generator.send((key, value, time))
            except StopIteration:
                self._error = True

        for resp in ("start", "format_done", "init_done", "verify_done", "reset_complete"):
            self.register_callback(resp, run_gen)

    def teardown(self):
        """No work to do here"""
        pass

    def kvstore_resilience_test(self):
        """Generator for running the test
        
        This function calls yield to wait for the next event from
        the device. If the device gives the wrong response, then the
        generator terminates by returing which raises a StopIteration
        exception and fails the test.
        """

        # Wait for start token
        key, value, time = yield
        if key != "start":
            return

        for type in ("Internal", "TDB-External", "File-System"):

            # Format the device before starting the test
            self.send_kv("format", type)
            key, value, time = yield
            if key != "format_done":
                return

            for i in range(self.RESET_COUNT):

                self.send_kv("init", type)
                key, value, time = yield
                if key != "init_done":
                    return

                self.send_kv("verify", i)
                key, value, time = yield
                if key != "verify_done":
                    return

                if i < self.RESET_COUNT - 1:
                    self.send_kv("run", i + 1)
                    sleep(self.RESET_DELAY_BASE +  random.uniform(0.0, 2.0))

                self.reset()

                # Wait for reset complete token
                key, value, time = yield
                if key != "reset_complete":
                    return

                self.send_kv("__sync", "00000000-0000-000000000-000000000000")

                # Wait for start token
                key, value, time = yield
                if key != "start":
                    return

        self.send_kv("exit", "pass")

        yield    # No more events expected
