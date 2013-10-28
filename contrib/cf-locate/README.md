# cf-locate

This is a small perl script to help you locate and display bodies or bundles inside of your masterfiles.


## Example

    cf-locate sync_cp /var/cfengine/masterfiles
    definition for sync_cp found in /var/cfengine/masterfiles/libraries/cfengine_stdlib.cf on line 1219

    body copy_from sync_cp(from,server)
    {
    servers     => { "$(server)" };
    source      => "$(from)";
    purge       => "true";
    preserve    => "true";
    type_check  => "false";
    }

    cf-locate u_rcp /var/cfengine/masterfiles
    definition for u_rcp found in /var/cfengine/masterfiles/update.cf on line 226

    body copy_from u_rcp(from,server)
    {
     source      => "$(from)";
     compare     => "digest";
     trustkey    => "false";

    !am_policy_hub::

     servers => { "$(server)" };
    }

