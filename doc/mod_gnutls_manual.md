* * * * *

`mod_gnutls` is a module for the Apache web server that provides HTTPS
(HTTP over Transport Layer Security (TLS)) using the GnuTLS library.
More information about the module can be found at
[the project's website](https://mod.gnutls.org/).

* * * * *

Compilation & Installation
==========================

`mod_gnutls` uses the `./configure && make && make install` mechanism
common to many Open Source programs.  Most of the dirty work is
handled by either `./configure` or Apache's `apxs` utility. If you have
built Apache modules before, there shouldn't be any surprises for you.

The interesting options you can pass to configure are:

`--with-apxs=PATH` 
:   This option is used to specify the location of the apxs utility that
    was installed as part of apache. Specify the location of the
    binary, not the directory it is located in.

`--with-apu-config=PATH`
:   Path to APR Utility Library config tool (`apu-1-config`)

`--help`
:   Provides a list of all available configure options.

It is recommended to run `make check` before installation. If your
system doesn't have a loopback device with IPv6 and IPv4 support or
`localhost` does not resolve to at least one of `[::1]` and
`127.0.0.1`, you may have to set the `TEST_HOST` or `TEST_IP`
environment variables when running `./configure` to make the test
suite work correctly.

* * * * *

Integration
===========

To activate `mod_gnutls` just add the following line to your httpd.conf
and restart Apache:

    LoadModule gnutls_module modules/mod_gnutls.so

Module Dependencies
-------------------

`mod_gnutls` uses the Apache HTTPD [Shared Object
Cache](http://httpd.apache.org/docs/current/en/socache.html) to cache
[OCSP responses for OCSP stapling](#gnutlsocspcache) and [TLS
sessions](#gnutlscache). To use either cache you need to load a
suitable `mod_socache_PROVIDER` module, which should be provided by
your Apache installation.

It is recommended to load at least `mod_socache_shmcb`. If that module
is loaded `mod_gnutls` will [enable OCSP stapling by
default](#gnutlsocspstapling), without needing any further
configuration other than a [certificate chain](#gnutlscertificatefile)
with OCSP support.

Note on HTTP/2
--------------

HTTP/2 is supported with `mod_gnutls`. However, full support requires
compiling with GnuTLS 3.6.3 or later. When using lower versions all
virtual hosts using `mod_gnutls` with overlapping IP/port combinations
need to use identical `Protocols` directives for protocol negotiation
to work correctly.

The technical reason is that using HTTP/2 requires ALPN (Application
Layer Protocol Negotiation) to be set up before GnuTLS parses the TLS
ClientHello message, but earlier hooks cannot use
`gnutls_server_name_get()` to retrieve SNI (Server Name Indication)
data for virtual host selection. Because of this `mod_gnutls` provides
its own early SNI parser, which requires the `gnutls_ext_raw_parse()`
function introduced in GnuTLS 3.6.3 to retrieve the extension data in
a *pre* client hello hook.

During build `./configure` will report "Early SNI: yes" if your
version of GnuTLS is new enough.

* * * * *

Configuration Directives
========================

General Options
---------------

### GnuTLSEnable

Enable GnuTLS for this virtual host

    GnuTLSEnable [on|off]

Default: *off*\
Context: virtual host

This directive enables SSL/TLS Encryption for a Virtual Host.

### GnuTLSCache

Configure TLS Session Cache

    GnuTLSCache (shmcb|dbm|memcache|...|none)[:PARAMETERS]

Default: `GnuTLSCache none`\
Context: server config

This directive configures the TLS Session Cache for `mod_gnutls`. This
could be shared between machines of different architectures. If the
selected cache implementation is not thread-safe, access is serialized
using the `gnutls-cache` mutex.

Which cache implementations are available depends on your Apache
installation and configuration, `mod_gnutls` can use any socache
provider. In general you will need to load a `mod_socache_PROVIDER`
module. Common options are described below, please check the Apache
HTTPD documentation for details on available providers and their
configuration.

`shmcb`
:   Uses a shared memory segment. This is a high performance local
    cache. The parameter is a relative or absolute path to be used if
    the local shared memory implementation requires one, followed by
    the cache size in bytes enclosed in parentheses.

    Example: `shmcb:cache/gnutls_cache(65536)`

`dbm`
:   Uses a DBM cache file. The parameter is a relative or absolute
    path to be used as the DBM cache file.

    Example: `dbm:cache/gnutls_cache`

`memcache`
:   Uses memcached server(s) to cache TLS session data. The parameter
    is a comma separated list of servers (host:port). This can be used
    to share a session cache between all servers in a cluster.

    Example: `memcache:memcache.example.com:12345,memcache2.example.com:12345`

`none`
:   Turns off all caching of TLS sessions.

    This can significantly reduce the performance of `mod_gnutls`
    since even followup connections by a client must renegotiate
    parameters instead of reusing old ones. This is the default, since
    it requires no configuration.

    Session tickets are an alternative to using a session cache,
    please see `GnuTLSSessionTickets`. Note that for TLS 1.3 GnuTLS
    supports resumption using session tickets only as of version
    3.6.4.

### GnuTLSCacheTimeout

Timeout for TLS Session Cache expiration

    GnuTLSCacheTimeout SECONDS

Default: `GnuTLSCacheTimeout 300`\
Context: server config, virtual host

Sets the expiration timeout for cached TLS sessions.

### GnuTLSSessionTickets

Enable Session Tickets for the server

    GnuTLSSessionTickets [on|off]

Default: `on` with GnuTLS 3.6.4 and newer, `off` otherwise\
Context: server config, virtual host

Session tickets allow TLS session resumption without session state
stored on the server, using encrypted tickets provided to the clients
instead. Tickets are an alternative to using a session cache, and
currently the only session resumption mechanism in TLS 1.3. For a pool
of servers this option is not recommended since the tickets are bound
to the issuing server only.

If this option is set in the global configuration, virtual hosts
without a `GnuTLSSessionTickets` setting will use the global setting.

*Warning:* With GnuTLS version before 3.6.4 the master key that
protects the tickets is generated only on server start, and there is
no mechanism to roll over the key. If session tickets are enabled it
is highly recommended to restart the server regularly to protect past
sessions in case an attacker gains access to server memory. GnuTLS
3.6.4 introduced an automatic TOTP-based key rollover, so this warning
does not apply any more and tickets are enabled by default.

### GnuTLSClientVerify

Enable Client Certificate Verification

    GnuTLSClientVerify [ignore|request|require]

Default: `ignore`\
Context: server config, virtual host, directory, .htaccess

This directive controls the use of TLS Client Certificate
Authentication. If used in the .htaccess context, it can force TLS
re-negotiation.

`ignore`
:   `mod_gnutls` will ignore the contents of any TLS Client Certificates
    sent. It will not request that the client sends a certificate.

`request`
:   The client certificate will be requested, but not required.
    The Certificate will be validated if sent.  The output of the
    validation status will be stored in the `SSL_CLIENT_VERIFY`
    environment variable and can be `SUCCESS`, `FAILED` or `NONE`.

`require`
:   A Client certificate will be required. Any requests without a valid
    client certificate will be denied.  The `SSL_CLIENT_VERIFY`
    environment variable will only be set to `SUCCESS`.

### GnuTLSDHFile

Use the provided PKCS \#3 encoded Diffie-Hellman parameters

    GnuTLSDHFile FILEPATH

Default: *none*\
Context: server config, virtual host

By default, `mod_gnutls` uses the DH parameters included with GnuTLS
corresponding to the security level of the configured private keys if
compiled with GnuTLS 3.5.6 or newer, and the ffdhe2048 DH group as
defined in RFC 7919, Appendix A.1 otherwise.

If you need to use different DH parameters, you can provide a PEM file
containing them in PKCS \#3 encoding using this option. Please see the
"[Parameter
generation](https://gnutls.org/manual/html_node/Parameter-generation.html)"
section of the GnuTLS documentation for a short discussion of the
security implications.

### GnuTLSPriorities

Set the allowed protocol versions, ciphers, key exchange algorithms,
MACs and compression methods

    GnuTLSPriorities NORMAL:+CIPHER_0:+CIPHER_1:...:+CIPHER_N

Default: `NORMAL`\
Context: server config, virtual host

Sets the allowed protocol version(s), ciphers, key exchange methods,
message authentication codes, and other TLS parameters for the server.
The parameter is a GnuTLS priority string as described in the
[the GnuTLS documentation](https://gnutls.org/manual/html_node/Priority-Strings.html).

For example, to disable TLS 1.0 use `NORMAL:-VERS-TLS1.0`.

### GnuTLSP11Module

Load this PKCS #11 module.

    GnuTLSP11Module PATH_TO_LIBRARY

Default: *none*\
Context: server config

Load this PKCS #11 provider module, instead of the system
defaults. May occur multiple times to load multiple modules.

### GnuTLSPIN

Set the PIN to be used to access encrypted key files or PKCS #11 objects.

    GnuTLSPIN XXXXXX

Default: *none*\
Context: server config, virtual host

Takes a string to be used as a PIN for the protected objects in
a security module, or as a key to be used to decrypt PKCS #8, PKCS #12,
or openssl encrypted keys.

### GnuTLSSRKPIN

Set the SRK PIN to be used to access the TPM.

    GnuTLSSRKPIN XXXXXX

Default: *none*\
Context: server config, virtual host

Takes a string to be used as a PIN for the protected objects in
the TPM module.

### GnuTLSExportCertificates

Export the PEM encoded certificates to CGIs

    GnuTLSExportCertificates [off|on|SIZE]

Default: `off`\
Context: server config, virtual host

This directive configures exporting the full certificates of the
server and the client to CGI scripts via the `SSL_SERVER_CERT` and
`SSL_CLIENT_CERT` environment variables. The exported certificates
will be PEM-encoded, limited to the given size. The type of the
certificate will be exported in `SSL_SERVER_CERT_TYPE` and
`SSL_CLIENT_CERT_TYPE`.

SIZE should be an integer number of bytes, or may be written with a
trailing `K` to indicate kibibytes.  `off` means the same thing as
`0`, in which case the certificates will not be exported to the
environment. `on` is an alias for `16K`. If a non-zero size is
specified for this directive, but a certificate is too large to fit in
the buffer, then the corresponding environment variable will contain
the fixed string `GNUTLS_CERTIFICATE_SIZE_LIMIT_EXCEEDED`.

With GnuTLSExportCertificates enabled, `mod_gnutls` exports the same
environment variables to the CGI process as `mod_ssl`.

X.509 Certificate Authentication
--------------------------------

### GnuTLSCertificateFile

Set the PEM encoded server certificate or certificate chain

    GnuTLSCertificateFile FILEPATH

Default: *none*\
Context: server config, virtual host

FILEPATH is an absolute or relative path to a file containing the
PEM-encoded X.509 certificate to use as this Server's End Entity (EE)
certificate, and optionally those of the issuing Certificate
Authorities (CAs). If the file contains multiple certificates they
must be ordered from EE to the CA closest to the root CA (or the root
CA itself).

Including at least the immediately issuing CA is highly recommended
because it is required for OCSP stapling.

Since version 0.7 this can be a PKCS #11 URL instead of a file.

On Linux and other Unix-like systems you can create the file with a
command like this (assuming "CA 1" issued the server certificate and
has been issued by "Root CA" itself):

	$ cat server.pem ca-1.pem root-ca.pem >server-chain.pem

### GnuTLSKeyFile

Set to the PEM Encoded Server Private Key

    GnuTLSKeyFile FILEPATH

Default: *none*\
Context: server config, virtual host

Takes an absolute or relative path to the Server Private Key. Set
`GnuTLSPIN` if the key file is encrypted.

Since version 0.7 this can be a PKCS #11 URL.

**Security Warning:**\
This private key must be protected. It is read while Apache is still
running as root, and does not need to be readable by the nobody or
apache user.

### GnuTLSClientCAFile

Set the PEM encoded Certificate Authority list to use for X.509 base
client authentication

    GnuTLSClientCAFile FILEPATH

Default: *none*
Context: server config, virtual host

Takes an absolute or relative path to a PEM Encoded Certificate to use
as a Certificate Authority with Client Certificate Authentication.
This file may contain a list of trusted authorities.

SRP Authentication
------------------

### GnuTLSSRPPasswdFile

Set to the SRP password file for SRP ciphersuites

    GnuTLSSRPPasswdFile FILEPATH

Default: *none*\
Context: server config, virtual host

Takes an absolute or relative path to an SRP password file. This is
the same format as used in libsrp.  You can generate such file using
the command `srptool --passwd /etc/tpasswd --passwd-conf
/etc/tpasswd.conf -u test` to set a password for user test.  This
password file holds the username, a password verifier and the
dependency to the SRP parameters.

### GnuTLSSRPPasswdConfFile

Set to the SRP password.conf file for SRP ciphersuites

    GnuTLSSRPPasswdConfFile FILEPATH

Default: *none*\
Context: server config, virtual host

Takes an absolute or relative path to an SRP password.conf file. This
is the same format as used in `libsrp`.  You can generate such file
using the command `srptool --create-conf /etc/tpasswd.conf`.  This
file holds the SRP parameters and is associate with the password file
(the verifiers depends on these parameters).

TLS Proxy Configuration
-----------------------

### GnuTLSProxyEngine

Enable TLS proxy connections for this virtual host

    GnuTLSProxyEngine [on|off]

Default: *off*\
Context: virtual host

This directive enables support for TLS proxy connections for a virtual
host.

### GnuTLSProxyCAFile

Set to the PEM encoded Certificate Authority Certificate

    GnuTLSProxyCAFile FILEPATH

Default: *none*\
Context: server config, virtual host

Takes an absolute or relative path to a PEM encoded certificate to use
as a Certificate Authority when verifying certificates provided by
proxy back end servers. This file may contain a list of trusted
authorities. If not set, verification of TLS back end servers will
always fail due to lack of a trusted CA.

### GnuTLSProxyCRLFile

Set to the PEM encoded Certificate Revocation List

    GnuTLSProxyCRLFile FILEPATH

Default: *none*\
Context: server config, virtual host

Takes an absolute or relative path to a PEM encoded Certificate
Revocation List to use when verifying certificates provided by proxy
back end servers. The file may contain a list of CRLs.

### GnuTLSProxyCertificateFile

Set to the PEM encoded Client Certificate

    GnuTLSProxyCertificateFile FILEPATH

Default: *none*\
Context: server config, virtual host

Takes an absolute or relative path to a PEM encoded X.509 certificate
to use as this Server's End Entity (EE) client certificate for TLS
client authentication in proxy TLS connections. If you need to supply
certificates for intermediate Certificate Authorities (iCAs), they
should be listed in sequence in the file, from EE to the iCA closest
to the root CA. Optionally, you can also include the root CA's
certificate as the last certificate in the list.

If not set, TLS client authentication will be disabled for TLS proxy
connections. If set, `GnuTLSProxyKeyFile` must be set as well to
provide the matching private key.

### GnuTLSProxyKeyFile

Set to the PEM encoded Private Key

    GnuTLSProxyKeyFile FILEPATH

Default: *none*\
Context: server config, virtual host

Takes an absolute or relative path to the Private Key matching the
certificate configured using the `GnuTLSProxyCertificateFile`
directive. This key cannot currently be password protected.

**Security Warning:**\
This private key must be protected. It is read while Apache is still
running as root, and does not need to be readable by the nobody or
apache user.

### GnuTLSProxyPriorities

Set the allowed ciphers, key exchange algorithms, MACs and compression
methods for proxy connections

    GnuTLSProxyPriorities NORMAL:+CIPHER_0:+CIPHER_1:...:+CIPHER_N

Default: `NORMAL`\
Context: server config, virtual host

Sets the allowed protocol version(s), ciphers, key exchange methods,
message authentication codes, and other TLS parameters for TLS proxy
connections. Like for `GnuTLSPriorities` the parameter is a GnuTLS
priority string as described in the
[the GnuTLS documentation](https://gnutls.org/manual/html_node/Priority-Strings.html).

OCSP Stapling Configuration
---------------------------

### GnuTLSOCSPStapling

Enable OCSP stapling for this (virtual) host.

    GnuTLSOCSPStapling [On|Off]

Default: *on* if requirements are met, *off* otherwise\
Context: server config, virtual host

OCSP stapling, formally known as the TLS Certificate Status Request
extension, allows the server to provide the client with a cached OCSP
response for its certificate during the handshake. With OCSP stapling
the client does not have to send an OCSP request to the issuer CA to
check the certificate status, which offers privacy and performance
advantages, and avoids the security issue of how to handle errors that
prevent the client from getting a response.

Using OCSP stapling has a few requirements:

* `GnuTLSCertificateFile` must contain the issuer CA certificate in
  addition to the server certificate so responses can be verified.
* The server certificate must either contain an OCSP access URI using
  HTTP, or `GnuTLSOCSPResponseFile` must be set.
* Caching OCSP responses requires a cache to store responses. If
  `mod_socache_shmcb` is loaded `mod_gnutls` can set up the cache
  automatically without additional configuration, see
  `GnuTLSOCSPCache`.

Stapling is activated by default if these requirements are met. If
`GnuTLSOCSPStapling` is explicitly set to `on` unmet requirements are
an error.

OCSP cache updates are serialized using the `gnutls-ocsp` mutex.

### GnuTLSOCSPCache

OCSP stapling cache configuration

	GnuTLSOCSPCache (shmcb|memcache|...|none)[:PARAMETERS]

Default: `shmcb:gnutls_ocsp_cache`\
Context: server config

This directive configures the OCSP stapling cache, and uses the same
syntax as `GnuTLSOCSPCache`. Please check there for details.

The default should be reasonable for most servers and requires
[mod\_socache\_shmcb](http://httpd.apache.org/docs/current/en/mod/mod_socache_shmcb.html)
to be loaded. Servers with very many virtual hosts may need to
increase the default cache size via the parameters string, those with
few virtual hosts and memory constraints could save a few KB by reducing
it. Note that `mod_socache_dbm` has a size constraint for entries that
is generally too small for OCSP responses.

If the selected cache implementation is not thread-safe, access
is serialized using the `gnutls-ocsp-cache` mutex.

### GnuTLSOCSPAutoRefresh

Regularly refresh cached OCSP response independent of TLS handshakes?

    GnuTLSOCSPAutoRefresh [On|Off]

Default: *on*\
Context: server config, virtual host

By default `mod_gnutls` will regularly refresh the cached OCSP
response for hosts that have OCSP stapling enabled, regardless of
whether it is used. This has advantages over updating the OCSP
response only if a TLS handshake needs it:

* Updating the cached response before it expires can hide short
  unavailability of the OCSP responder, if a repeated request is
  successful before the cache expires (see below).

* Handshakes are not slowed down by fetching responses.

The interval to the next request is determined as follows: After a
successful OCSP request the next one is scheduled for a random period
between `GnuTLSOCSPFuzzTime` and half of it before
`GnuTLSOCSPCacheTimeout` expires. For example, if the cache timeout is
3600 seconds and the fuzz time 600 seconds, the next request will be
sent after 3000 to 3300 seconds. If the validity period of the
response expires before then, the selected interval is halved until it
is smaller than the time until expiry. If an OCSP request fails, it is
retried after `GnuTLSOCSPFailureTimeout`.

Regularly updating the OCSP cache requires `mod_watchdog`,
`mod_gnutls` will fall back to updating the OCSP cache during
handshakes if `mod_watchdog` is not available or this option is set to
`Off`.

### GnuTLSOCSPCheckNonce

Check the nonce in OCSP responses?

    GnuTLSOCSPCheckNonce [On|Off]

Default: *on*\
Context: server config, virtual host

Some CAs refuse to send nonces in their OCSP responses, probably
because that way they can cache responses. If your CA is one of them
you can use this flag to disable nonce verification. Note that
`mod_gnutls` will _send_ a nonce either way.

### GnuTLSOCSPResponseFile

Read the OCSP response for stapling from this file instead of sending
a request over HTTP.

    GnuTLSOCSPResponseFile /path/to/response.der

Default: *empty*\
Context: server config, virtual host

The response file must be updated externally, for example using a cron
job. This option is an alternative to the server fetching OCSP
responses over HTTP. Reasons to use this option include:

* Performing OCSP requests separate from the web server.
* The issuer CA uses an access method other than HTTP.
* Testing

You can use a GnuTLS `ocsptool` command like the following to create
and update the response file:

    ocsptool --ask --nonce --load-issuer ca_cert.pem \
        --load-cert server_cert.pem --outfile ocsp_response.der

Additional error checking is highly recommended. You may have to
remove the `--nonce` option if the OCSP responder of your CA does not
support nonces.

### GnuTLSOCSPCacheTimeout

Cache timeout for OCSP responses

    GnuTLSOCSPCacheTimeout SECONDS

Default: *3600*\
Context: server config, virtual host

Cached OCSP responses will be refreshed after the configured number of
seconds. How long this timeout should reasonably be depends on your
CA, namely how often its OCSP responder is updated and how long
responses are valid. Note that a response will not be cached beyond
its lifetime as denoted in the `nextUpdate` field of the response.

### GnuTLSOCSPFailureTimeout

Wait this many seconds before retrying a failed OCSP request.

    GnuTLSOCSPFailureTimeout SECONDS

Default: *300*\
Context: server config, virtual host

Retries of failed OCSP requests must be rate limited to avoid
overloading both the server using mod_gnutls and the CA's OCSP
responder. A shorter value increases the load on both sides, a longer
one means that stapling will remain disabled for longer after a failed
request. The auto-refresh mechanism updates OCSP responses before they
expire and can cover short unavailability of OCSP responders, see
[`GnuTLSOCSPAutoRefresh`](#gnutlsocspautorefresh) for details.

### GnuTLSOCSPFuzzTime

Update the cached OCSP response up to this time before the cache expires

    GnuTLSOCSPFuzzTime SECONDS

Default: *larger of GnuTLSOCSPCacheTimeout / 8 and GnuTLSOCSPFailureTimeout \* 2*\
Context: server config, virtual host

Refreshing the cached response before it expires hides short OCSP
responder unavailability. See `GnuTLSOCSPAutoRefresh` for how this
value is used, using at least twice `GnuTLSOCSPFailureTimeout` is
recommended.

### GnuTLSOCSPSocketTimeout

Timeout for TCP sockets used to send OCSP requests

    GnuTLSOCSPFailureTimeout SECONDS

Default: *6*\
Context: server config, virtual host

Stalled OCSP requests must time out after a while to prevent stalling
the server too much. However, if the timeout is too short requests may
fail with a slow OCSP responder or high latency network
connection. This parameter allows you to adjust the timeout if
necessary.

Note that this is not an upper limit for the completion of an OCSP
request but a socket timeout. The connection will time out if there is
no activity (successful send or receive) at all for the configured
time.

* * * * *

Configuration Examples
======================

Minimal Example
---------------

A minimal server configuration using mod_gnutls might look like this
(other than the default setup):

```apache
# Load mod_gnutls into Apache.
LoadModule gnutls_module modules/mod_gnutls.so

Listen 192.0.2.1:443

<VirtualHost _default_:443>
	# Standard virtual host stuff
	DocumentRoot /www/site1.example.com/html
	ServerName site1.example.com:443

	# Minimal mod_gnutls setup: enable, and set credentials
	GnuTLSEnable on
	GnuTLSCertificateFile conf/tls/site1_cert_chain.pem
	GnuTLSKeyFile conf/tls/site1_key.pem
</VirtualHost>
```

This gives you an HTTPS site using the GnuTLS `NORMAL` set of
ciphersuites. OCSP stapling will be enabled if the server certificate
contains an OCSP URI, `conf/tls/site1_cert_chain.pem` contains the
issuer certificate in addition to the server's, and
[mod\_socache\_shmcb](http://httpd.apache.org/docs/current/en/mod/mod_socache_shmcb.html)
is loaded. With Gnutls 3.6.4 or newer session tickets are enabled,
too.

Virtual Hosts with Server Name Indication
-----------------------------------------

`mod_gnutls` supports Server Name Indication (SNI), as specified in
[RFC 6066, Section 3](https://tools.ietf.org/html/rfc6066#section-3).
This allows hosting many TLS websites with a single IP address, you
can just add virtual host configurations. All recent browsers support
this standard. Here is an example using SNI:

```apache
# Load the module into Apache.
LoadModule gnutls_module modules/mod_gnutls.so
# This example server uses session tickets, no cache.
GnuTLSSessionTickets on

# SNI allows hosting multiple sites using one IP address. This
# could also be 'Listen *:443', just like '*:80' is common for
# non-HTTPS
Listen 198.51.100.1:443

<VirtualHost _default_:443>
	GnuTLSEnable on
	DocumentRoot /www/site1.example.com/html
    ServerName site1.example.com:443
	GnuTLSCertificateFile conf/tls/site1.crt
	GnuTLSKeyFile conf/tls/site1.key
</VirtualHost>

<VirtualHost _default_:443>
	GnuTLSEnable on
	DocumentRoot /www/site2.example.com/html
	ServerName site2.example.com:443
	GnuTLSCertificateFile conf/tls/site2.crt
	GnuTLSKeyFile conf/tls/site2.key
</VirtualHost>

<VirtualHost _default_:443>
	GnuTLSEnable on
	DocumentRoot /www/site3.example.com/html
	ServerName site3.example.com:443
	GnuTLSCertificateFile conf/tls/site3.crt
	GnuTLSKeyFile conf/tls/site3.key
	# Enable HTTP/2. With GnuTLS before version 3.6.3 all
	# virtual hosts in this example would have to share this
	# directive to work correctly.
	Protocols h2 http/1.1
</VirtualHost>
```

Virtual Hosts without SNI
-------------------------

If you need to support clients that do not use SNI, you have to use a
unique IP address/port combination for each virtual host. In this
example all virtual hosts use the default port for HTTPS (443) and
different IP addresses.

```apache
# Load the module into Apache.
LoadModule gnutls_module modules/mod_gnutls.so
# This example server uses a session cache.
GnuTLSCache dbm:/var/cache/www-tls-cache
GnuTLSCacheTimeout 1200

# Without SNI you need one IP Address per site. The IP addresses
# are listed separately for clarity, you could also use "Listen 443"
# to use that port on all available IP addresses.
Listen 192.0.2.1:443
Listen 192.0.2.2:443
Listen 192.0.2.3:443

<VirtualHost 192.0.2.1:443>
	GnuTLSEnable on
	GnuTLSPriorities SECURE128
	DocumentRoot /www/site1.example.com/html
	ServerName site1.example.com:443
	GnuTLSCertificateFile conf/tls/site1.crt
	GnuTLSKeyFile conf/tls/site1.key
</VirtualHost>

<VirtualHost 192.0.2.2:443>
    # This virtual host enables SRP authentication
	GnuTLSEnable on
	GnuTLSPriorities NORMAL:+SRP
	DocumentRoot /www/site2.example.com/html
	ServerName site2.example.com:443
	GnuTLSSRPPasswdFile conf/tls/tpasswd.site2
	GnuTLSSRPPasswdConfFile conf/tls/tpasswd.site2.conf
</VirtualHost>

<VirtualHost 192.0.2.3:443>
	# This server enables SRP and X.509 authentication.
	GnuTLSEnable on
	GnuTLSPriorities NORMAL:+SRP:+SRP-RSA:+SRP-DSS
	DocumentRoot /www/site3.example.com/html
	ServerName site3.example.com:443
	GnuTLSCertificateFile conf/tls/site3.crt
	GnuTLSKeyFile conf/tls/site3.key
	GnuTLSClientVerify ignore
	GnuTLSSRPPasswdFile conf/tls/tpasswd.site3
	GnuTLSSRPPasswdConfFile conf/tls/tpasswd.site3.conf
</VirtualHost>
```

OCSP Stapling Example
---------------------

This is an example with a customized OCSP stapling configuration. What
is a resonable cache timeout varies depending on how long your CA's
OCSP responses are valid. Some CAs provide responses that are valid
for multiple days, in that case timeout and fuzz time could be
significantly larger.

```apache
# Load the module into Apache.
LoadModule gnutls_module modules/mod_gnutls.so
# A 64K cache is more than enough for one response
GnuTLSOCSPCache shmcb:ocsp_cache(65536)

Listen 192.0.2.1:443

<VirtualHost _default_:443>
	GnuTLSEnable           On
	DocumentRoot           /www/site1.example.com/html
	ServerName             site1.example.com:443
	GnuTLSCertificateFile  conf/tls/site1_cert_chain.pem
	GnuTLSKeyFile          conf/tls/site1_key.pem
	GnuTLSOCSPStapling     On
	# The cached OCSP response is kept for up to 4 hours,
	# with updates scheduled every 3 to 3.5 hours.
	GnuTLSOCSPCacheTimeout 21600
	GnuTLSOCSPFuzzTime     3600
</VirtualHost>
```

* * * * *

Environment Variables
=====================

`mod_gnutls` exports the following environment variables to scripts.
These are compatible with `mod_ssl`.

`HTTPS`
-------

Can be `on` or `off`

`SSL_VERSION_LIBRARY`
---------------------

The version of the GnuTLS library

`SSL_VERSION_INTERFACE`
-----------------------

The version of this module

`SSL_PROTOCOL`
--------------

The SSL or TLS protocol name (such as `TLS 1.0` etc.)

`SSL_CIPHER`
------------

The SSL or TLS cipher suite name

`SSL_COMPRESS_METHOD`
---------------------

The negotiated compression method (`NULL` or `DEFLATE`)

`SSL_SRP_USER`
--------------

The SRP username used for authentication (only set when
`GnuTLSSRPPasswdFile` and `GnuTLSSRPPasswdConfFile` are configured).

`SSL_CIPHER_USEKEYSIZE` & `SSL_CIPHER_ALGKEYSIZE`
-------------------------------------------------

The number if bits used in the used cipher algorithm.

This does not fully reflect the security level since the size of
RSA or DHE key exchange parameters affect the security level too.

`SSL_DH_PRIME_BITS`
-------------------

The number if bits in the modulus for the DH group, if DHE or static
DH is used.

This will not be set if DH is not used.

`SSL_CIPHER_EXPORT`
-------------------

`True` or `False`. Whether the cipher suite negotiated is an export one.

`SSL_SESSION_ID`
----------------

The session ID negotiated in this session. Can be the same during client
reloads.

`SSL_CLIENT_V_REMAIN`
---------------------

The number of days until the client's certificate is expired.

`SSL_CLIENT_V_START`
--------------------

The activation time of client's certificate.

`SSL_CLIENT_V_END`
------------------

The expiration time of client's certificate.

`SSL_CLIENT_S_DN`
-----------------

The distinguished name of client's certificate in RFC2253 format.

`SSL_CLIENT_I_DN`
-----------------

The distinguished name of the issuer of the client's certificate in
RFC2253 format.

`SSL_CLIENT_S_AN%`
------------------

These will contain the alternative names of the client certificate (`%` is
a number starting from zero).

The values will be prepended by `DNSNAME:`, `RFC822NAME:` or `URI:`
depending on the type.

If it is not supported the value `UNSUPPORTED` will be set.

`SSL_SERVER_M_SERIAL`
---------------------

The serial number of the server's certificate.

`SSL_SERVER_M_VERSION`
----------------------

The version of the server's certificate.

`SSL_SERVER_A_SIG`
------------------

The algorithm used for the signature in server's certificate.

`SSL_SERVER_A_KEY`
------------------

The public key algorithm in server's certificate.

`SSL_SERVER_CERT`
------------------

The PEM-encoded (X.509) server certificate (see the
`GnuTLSExportCertificates` directive).

`SSL_SERVER_CERT_TYPE`
----------------------

The certificate type will be `X.509`.

`SSL_CLIENT_CERT`
------------------

PEM-encoded (X.509) client certificate, if any (see the
`GnuTLSExportCertificates` directive).

`SSL_CLIENT_CERT_TYPE`
----------------------

The certificate type will be `X.509`, if any.