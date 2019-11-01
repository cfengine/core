# libcfnet - CFEngine Network protocol

Generally, details about the protocol are explained in comments / code.
However, some explanations don't naturally fit in one part / file of the codebase.
So, they are provided here.


## Network protocol versioning


### Protocol versions

Names of protocol versions:

1. `"classic"` - Legacy, pre-TLS, protocol. Not enabled or allowed by default.
2. `"tls"` - TLS Protocol using OpenSSL. Encrypted and 2-way authentication.
3. `"cookie"` - TLS Protocol with cookie command for duplicate host detection.

Wanted protocol version can be specified from policy:

```
body copy_from protocol_latest
{
  protocol_version => "latest";
}
```

Additionally, numbers can also be used:

```
body copy_from protocol_three
{
  protocol_version => "3";
}
```


### Version negotiation

Client side (`cf-agent`, `cf-hub`, `cf-runagent`, `cf-net`) uses `ServerConnection()` function to connect to a server.
Server side (`cf-serverd`, `cf-testd`) uses `ServerTLSPeek()` to check if the connection is TLS or not, distinguishing version 1 and 2.
Protocol version 1 is not allowed by default, but can be allowed using `allowlegacyconnects`.
Version negotiation then happens inside `ServerIdentificationDialog()` (server side) and `ServerConnection()` (client side).
Client requests a wanted version, by sending a version string, for example:

```
CFE_v3
```

The version requested is usually the latest supported, unless specified in policy (`body copy_from`).
Then, the server responds with the highest supported version (but not higher than the requested version).
For a 3.12 server, this would be:

```
CFE_v2
```

Both server and client will then set `conn_info->protocol` to `2`, and use protocol version 2.
There is currently no way to require a specific version number (only allow / disallow version 1).
This is because version 2 and 3 are practically identical.
Downgrade from version 3 to 2 happens seamlessly, but crucially, it doesn't downgrade to version 1 inside the TLS code.
