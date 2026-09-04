{ stdenv
, version
, is-enterprise
, is-hub
, lib
, pkg-config
, autoreconfHook
, bison
, flex
, diffutils
, coreutils
, acl
, attr
, curl
, libxml2
, libyaml
, lmdb
, openssl
, pcre2
, zlib
, librsync
, pam
, perl
, shadow
}:

stdenv.mkDerivation {
  pname = "cfengine";
  version = version;
  src = ../../.;

  postPatch = ''
    echo "${version}" > CFVERSION
  '';

  preBuild = ''
    patchShebangs .
  '';

  nativeBuildInputs = [
    pkg-config
    autoreconfHook
    bison
    flex
    diffutils
    perl
    shadow
  ];

  buildInputs = [
    acl
    attr
    curl
    libxml2
    libyaml
    lmdb
    openssl
    pcre2
    zlib
    pam
    librsync
  ];

  enableParallelBuilding = true;

  configureFlags = [
    "--with-lmdb"
    "--enable-debug"
  ];

  postInstall = ''
    cp ${coreutils}/bin/date $out/bin/date
    cp ${diffutils}/bin/diff $out/bin/diffutils
  '';
}
