from __future__ import print_function
from __future__ import absolute_import

import signal
from multiprocessing import Process

import greenlet
from . import _test_extension_cpp
from . import TestCase

def run_unhandled_exception_in_greenlet_aborts():
    # This is used in multiprocessing.Process and must be picklable
    # so it needs to be global.


    def _():
        _test_extension_cpp.test_exception_switch_and_do_in_g2(
            _test_extension_cpp.test_exception_throw
        )
    g1 = greenlet.greenlet(_)
    g1.switch()

class CPPTests(TestCase):
    def test_exception_switch(self):
        greenlets = []
        for i in range(4):
            g = greenlet.greenlet(_test_extension_cpp.test_exception_switch)
            g.switch(i)
            greenlets.append(g)
        for i, g in enumerate(greenlets):
            self.assertEqual(g.switch(), i)

    def _do_test_unhandled_exception(self, target):
        # TODO: On some versions of Python with some settings, this
        # spews a lot of garbage to stderr. It would be nice to capture and ignore that.
        import sys
        WIN = sys.platform.startswith("win")

        p = Process(target=target)
        p.start()
        p.join(10)
        # The child should be aborted in an unusual way. On POSIX
        # platforms, this is done with abort() and signal.SIGABRT,
        # which is reflected in a negative return value; however, on
        # Windows, even though we observe the child print "Fatal
        # Python error: Aborted" and in older versions of the C
        # runtime "This application has requested the Runtime to
        # terminate it in an unusual way," it always has an exit code
        # of 3. This is interesting because 3 is the error code for
        # ERROR_PATH_NOT_FOUND; BUT: the C runtime abort() function
        # also uses this code.
        #
        # If we link to the static C library on Windows, the error
        # code changes to '0xc0000409' (hex(3221226505)), which
        # apparently is STATUS_STACK_BUFFER_OVERRUN; but "What this
        # means is that nowadays when you get a
        # STATUS_STACK_BUFFER_OVERRUN, it doesn’t actually mean that
        # there is a stack buffer overrun. It just means that the
        # application decided to terminate itself with great haste."
        #
        # See
        # https://devblogs.microsoft.com/oldnewthing/20110519-00/?p=10623
        # and
        # https://docs.microsoft.com/en-us/previous-versions/k089yyh0(v=vs.140)?redirectedfrom=MSDN
        # and
        # https://devblogs.microsoft.com/oldnewthing/20190108-00/?p=100655
        expected_exit = (
            -signal.SIGABRT,
            # But beginning on Python 3.11, the faulthandler
            # that prints the C backtraces sometimes segfaults after
            # reporting the exception but before printing the stack.
            # This has only been seen on linux/gcc.
            -signal.SIGSEGV,
        ) if not WIN else (
            3,
            0xc0000409,
        )
        self.assertIn(p.exitcode, expected_exit)

    def test_unhandled_exception_aborts(self):
        # verify that plain unhandled throw aborts
        self._do_test_unhandled_exception(_test_extension_cpp.test_exception_throw)


    def test_unhandled_exception_in_greenlet_aborts(self):
        # verify that unhandled throw called in greenlet aborts too
        self._do_test_unhandled_exception(run_unhandled_exception_in_greenlet_aborts)


if __name__ == '__main__':
    __import__('unittest').main()
