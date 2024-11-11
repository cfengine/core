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
3. `"filestream"` - Introduces a new streaming API for get file request (powered by librsync).

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

## Commands

### `GET <FILENAME>` (protocol v4)

The following is a description of the `GET <FILENAME>` command, modified in
protocol version v4 (introduced in CFEngine 3.25).

The initial motivation for creating a new protocol version `filestream` was
due to a race condition found in the `GET <FILENAME>` request. It relied on the
file size aquired by `STAT <FILENAME>`. However, if the file size increased
between the two requests, the client would think that the remaining data at the
offset of the aquired file size is a new protocol header. This situation would lead
to undefined behaviour. Hence, we needed a new protocol to send files. Instead
of reinventing the wheel, we decided to use librsync which utilizes the RSYNC
protocol to transmit files.

The server implementation is found in function
[CfGet()](../cf-serverd/server_common.c). Client impementations are found in
[CopyRegularFileNet()](client_code.c) and [ProtocolGet()](protocol.c)

Similar to before, the client issues a `GET <FILENAME>` request. However,
instead of continuing to execute the old protocol, the client immediately calls
`FileStreamFetch()` from the "File Stream API". Upon receiving such a request,
the server calls either `FileStreamRefuse()` (to refuse the request) or
`FileStreamServe()` (to comply with the request). The internal workings of the
File Stream API is well explained in [file_stream.h](file_stream.h).
