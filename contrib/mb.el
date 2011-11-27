;;;
;;; C indentation, Burgess style.
;;;
;;; Author: Thomas Sevaldrud.
;;; Changes: Mikhail Gusarov.
;;;
(defconst burgess-c-style
  '((c-tab-always-indent . t)
    (c-hanging-braces-alist . ((substatement-open before after)
                               (brace-list-open)))
    (c-hanging-colons-alist . ((member-init-intro before)
                               (inher-intro)
                               (case-label after)
                               (label after)
                               (access-label after)))
    (c-cleanup-list . (scope-operator))
    (c-offsets-alist . ((arglist-close . c-lineup-arglist)
                        (defun-block-intro . 0)
                        (substatement-open . 3)
                        (statement-cont . 3)
                        (statement-block-intro . 0)
                                (case-label . 0)
                                (block-open . 0)
                                (brace-list-open . 3)
                                (brace-list-intro . 0)
                                (class-open . 3)
                                (class-close . 3)
                                (inclass . 0)
                                (statement-case-open . 3)
                                (statement-case-intro . 3)
                                (knr-argdecl-intro . -))))
  "Burgess Programming Style")

(c-add-style "BURGESS" burgess-c-style)
