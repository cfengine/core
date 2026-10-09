{ stdenv
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
, version
}:

stdenv.mkDerivation {
  pname = "cfengine-core";
  version = version;
  src = ../../.;
  outputs = [ "out" "buildTree" ];

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

    mkdir -p "$buildTree"
    cp -a . "$buildTree"/
  '';
}
