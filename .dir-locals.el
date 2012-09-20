;;; .dir-locals.el --- Emacs 23.x and higher settings

((nil . ((indent-tabs-mode . nil)
         (tab-width . 4)
         (fill-column . 80)))
 (c-mode . ((eval . (unless (featurep 'cfengine-code-style)
                      (message "You should load cfengine-code-style.el")))
            (c-file-style . "cfengine")))
 (cc-mode . ((c-file-style . "cfengine")))
 (c++-mode . ((c-file-style . "cfengine")))
 (java-mode . ((c-file-style . "cfengine"))))
