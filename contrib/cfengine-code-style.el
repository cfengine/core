;;; cfengine-code-style.el --- C code style for CFEngine project.

;; Author: Mikhail Gusarov <mikhail.gusarov@cfengine.com>
;; URL: https://github.com/cfengine/core

;; This file is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; version 3.

;; This file is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <http://www.gnu.org/licenses/>.

;;; Commentary:

;; Usage:
;;
;; To enable coding style for the current buffer:
;;
;;     M-x c-set-style cfengine
;;
;; To enable coding style permanently, create file .dir-locals.el with the
;; following contents in the directory with the source code:
;;
;;     ((c-mode . ((c-file-style . "cfengine"))))
;;

;;
;; TODO: special rule for C99 (Foo) { 1, 2, 3 } initializers.
;; TODO: special rule for whitespace between if/while/for and paren.
;;

;;; Code:

(defconst cfengine-c-style
  '(;; 4 spaces
    (c-basic-offset . 4)
    ;; Braces on new lines
    (c-hanging-braces-alist . ((substatement-open before after)
                               (statement-cont)
                               (brace-list-open)
                               (brace-list-close)
                               (brace-list-intro)
                               (brace-entry-open)))
    ;; Newline after 'case' label
    (c-hanging-colons-alist . ((case-label after)))
    ;; Don't place comma on a separate line after array/aggregate
    (c-cleanup-list . (list-close-comma))
    ;; Indentation
    (c-offsets-alist . ((defun-open . 0)
                        (defun-block-intro . +)
                        (substatement-open . 0)
                        (statement-cont . +))))
  "CFEngine Code Style")

(c-add-style "CFEngine" cfengine-c-style)

;;
;; CFEngine uses CapitalizedWords for function names
;;

(defun cfengine-c-mode-hook ()
  (if (eq c-file-style "CFEngine")
      (progn
        (setq indent-tabs-mode nil)
        (setq c-tab-always-indent t)
        (setq tab-width 4)
        (subword-mode 1))))

(add-hook 'c-mode-hook 'cfengine-c-mode-hook)

(provide 'cfengine-code-style)

;;; cfengine-code-style.el ends here
