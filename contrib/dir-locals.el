;;; .dir-locals.el --- Emacs 23.x and higher settings

((cperl-mode . ((indent-tabs-mode . nil)
                (tab-width . 4)
                (fill-column . 80)
                (cperl-indent-level . 4)
                (cperl-brace-offset . 0)
                (cperl-close-paren-offset . -1)
                (cperl-brace-imaginary-offset . 0)
                (cperl-continued-brace-offset . -1)
                (cperl-continued-statement-offset . 1)
                (cperl-fix-hanging-brace-when-indent . t)
                (cperl-extra-newline-before-brace . t)
                (cperl-merge-trailing-else . nil)))
 (c-mode . ((c-file-style . "cfengine")))
 (c++-mode . ((c-file-style . "cfengine"))) ; some weirdos like c++-mode better
 (cfengine-mode . ((indent-tabs-mode . nil)
                   (tab-width . 4)
                   (fill-column . 80))))
