# Unit Tests for Apache's mod_gnutls

Authors:
Daniel Kahn Gillmor <dkg@fifthhorseman.net>,
Fiona Klute <fiona.klute@gmx.de>

There are a lot of ways that a TLS-capable web server can go wrong.  I
want to at least test for some basic/common configurations.


## Running the tests

From the top level of the source, or from `test/` (where this README is),
just run:

```bash
$ make check
```

You can also run specific test cases by passing their script names to
make in the `TESTS` variable:

```bash
$ TESTS="test-03_cachetimeout_in_vhost.bash" make -e check
```

This should be handy when you're just trying to experiment with a new
test and don't want to wait for the full test suite to run.

The default configuration assumes that a loopback device with IPv4 and
IPv6 support is available (`TEST_IP="[::1] 127.0.0.1"`) and that
`TEST_HOST="localhost"` resolves to at least one of these
addresses. If this does not apply to your system, you can pass
different values to `./configure`, e.g. to use IPv4 only:

```bash
$ TEST_HOST="localhost" TEST_IP="127.0.0.1" ./configure
```

If tests fail due to expired certificates or PGP signatures, run

```bash
$ make mostlyclean
```

to delete them and create fresh ones on the next test run. You could
also use `make clean`, but in that case the keys will be deleted as
well and have to be recreated, too, which takes more time.


## Adding a Test

Please add more tests!

The simplest way to add a test is (from the directory containing this
file):

```bash
$ ./newtest
```

This will prompt you for a simple name for the test, copy a starting
set of files from `tests/00_basic`, and create a script which you can
add to the `test_scripts` variable in `Makefile.am` when your test is
ready for inclusion in the test suite. The files in the test directory
must be added to `EXTRA_DIST` in `tests/Makefile.am`.


## Implementation

Each test consists of a script in the directory containing this README
and a directory in tests/, which the test suite uses to spin up an
isolated Apache instance (or more, for tests that need a proxy or OCSP
responder) and try to connect to it with gnutls-cli and make a simple
HTTP 1.1 or 1.0 request.

Test directories usually contain the following files:

* `apache.conf` -- Apache configuration to be used

* `test.yml` -- Defines the client connection(s) including parameters
  for `gnutls-cli`, the request(s), and expected response(s). Please
  see the module documentation of [mgstest.tests](./mgstest/tests.py)
  for details, and [`sample_test.yml`](./sample_test.yml) and
  [`sample_fail.yml`](./sample_fail.yml) for examples.

* `backend.conf` [optional] -- Apache configuration for the proxy
  backend server, if any

* `ocsp.conf` [optional] -- Apache configuration for the OCSP
  responder, if any

* `fail.server` [optional] -- if this file exists, it means we expect
  the web server to fail to even start due to some serious
  configuration problem.

* `hooks.py` [optional] -- Defines hook functions that modify or
  override the default behavior of `runtest.py`. Please see the module
  documentation of [mgstest.hooks](./mgstest/hooks.py) for details.

The [`runtest.py`](./runtest.py) program is used to start the required
services send a request (or more) based on the files described
above. Note that currently some tests take additional steps in their
test scripts, though `hooks.py` is the preferred mechanism.

By default (if the `unshare` command is available and has the
permissions required to create network and user namespaces), each test
case is run inside its own network namespace. This avoids address and
port conflicts with other tests as well has the host system. Otherwise
the tests use a lock file to prevent port conflicts between
themselves.

When writing your own tests, make sure to call `runtest.py` through
`netns_py.bash` like the current tests do to ensure compatibility with
the namespace and lock file mechanisms.

## Robustness and Tuning

Here are some things that you might want to tune about the tests based
on your expected setup (along with the variables that can be passed to
"make check" to adjust them):

* They need a functioning loopback device.

* They expect to have ports 9932 (`TEST_PORT` as defined in
  `Makefile.am`) through 9936 available for test services to bind to,
  and open for connections on the addresses listed in `TEST_IP`.

* Depending on the compile time configuration of the Apache binary
  installed on your system you may need to load additional Apache
  modules. The recommended way to do this is to drop a configuration
  file into the `apache-conf/` directory. Patches to detect such
  situations and automatically configure the tests accordingly are
  welcome.

* If a machine is particularly slow or under heavy load, it's possible
  that tests fail for timing reasons. [`TEST_QUERY_TIMEOUT` (timeout
  for the HTTPS request in seconds)]

The first two of these issues are avoided when the tests are isolated
using network namespaces, which is the default (see "Implementation"
above). The `./configure` script tries to detect if namespaces can be
used (some Linux distributions disable them for unprivileged
users). If this detection returns a false positive or you do not want
to use namespace isolation for some other reason, you can run
configure with the `--disable-test-namespaces` option.

In some situations you may want to see the exact environment as
configured by make, e.g. if you want to manually run an Apache
instance with Valgrind using the same configuration as a test
case. Use `make show-test-env` to dump `AM_TESTS_ENVIRONMENT` to
stdout. If you want to load the test environment into the current bash
instance, you can use:

```bash
$ eval $(make show-test-env)
```

If you are building on an exotic architecture which does not support
flock (or timeouts using `flock -w`), `./configure` should detect that
and disable locking, or you can disable it manually by passing
`--disable-flock` to `./configure`. This will force serial execution
of tests, including environment setup.