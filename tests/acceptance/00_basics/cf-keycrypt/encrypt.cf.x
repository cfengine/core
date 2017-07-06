body common control
{
      inputs => { "../../default.cf.sub" };
      bundlesequence => { default("$(this.promise_filename)") };
      version => "1.0";
}

bundle agent test
{
  meta:

    "description"
      string => "Test that cf-keycrypt can still encrypt content encrypted at the time of initial implementation";

      # if this breaks then something may have changed with supported key types (this was originally introduced in 3.11.0).

  commands:
      "$(sys.cf_keycrypt)"
        args => "-e $(this.promise_dirname)/testkey.pub -i $(this.promise_dirname)/plaintext -o $(G.testfile)";
}

bundle agent check
{
  methods:
      "any"
        usebundle => dcs_check_diff("$(this.promise_dirname)/encrypted",
                                    "$(G.testfile)",
                                    "$(this.promise_filename)");
}
