# cf-keycrypt

*cf-keycrypt* is a utility for encrypting sensitive data for use on
CFEngine-managed hosts. It is using the existing CFEngine key pairs for strong
cryptography based on the combination of RSA and AES ciphers.

## File format

The file format used by *cf-keycrypt* has the following schema:

```
  ------------
  | Headers  |
  ------------
  | AES IV   |
  ------------
  | AES key  |
  ------------
  | data     |
  ------------
```

The header format is similar to HTTP headers -- colon-separated key-value pairs
each on one line:

`Key: Value\n`

The header section is terminated by a blank line.

Supported headers are:

  * `Version` (required) -- version of the file format to allow backwards
                            compatibility

The AES initialization vector is 16 bytes long (256 bits) and serves the purpose
of the seed for the CBC (Cipher Block Chain) mode of operation of the AES
cipher.

The AES key is a randomly generated AES key encrypted by the specific RSA public
key and is as long as the RSA public key, currently 256 bytes (2048 bits).

The future versions of *cf-keycrypt* are expected to support more headers,
multiple keys (encryption for multiple hosts in a single file) and varying key
sizes.