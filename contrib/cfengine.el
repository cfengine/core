;;; cfengine.el --- An Emacs major-mode for editing cfengine scripts
;;; Copyright (C) 1997 Rolf Ebert

;;; Authors: Rolf Ebert      <ebert@waporo.muc.de>
;;; Keywords: languages 
;;; Rolf Ebert's version: cfengine.el-V1_1

;;; This file is not part of GNU Emacs or XEmacs.

;; cfengine.el is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; cfengine.el is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with cfengine; see the file COPYING.  If not, write to the
;; Free Software Foundation, Inc., 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.

;; USAGE:

;; I have the following lines in my .emacs

;;  ; autoload when needed
;;  (autoload 'cfengine-mode "cfengine" "" t nil)

;;  ; start colour highlighting
;;  ; if GNU Emacs >= 19.31
;;  ;   (global-font-lock-mode t)
;;  ; if XEmacs
;;  (add-hook 'cfengine-mode-hook 'turn-on-font-lock)

;;  ; detect file type by name (cf.*)
;;  (setq auto-mode-alist (append '(("cf\\." . cfengine-mode))
;;                                auto-mode-alist))



(defvar cfengine-indent 3
  "*Defines the size of Cfengine indentation.")

(defvar cfengine-mode-abbrev-table nil
  "Abbrev table used in Cfengine mode.")
(define-abbrev-table 'cfengine-mode-abbrev-table ())

(defvar cfengine-mode-map ()
  "Local keymap used for Cfengine mode.")

(defvar cfengine-mode-syntax-table nil
  "Syntax table to be used for editing Cfengine source code.")

(defvar cfengine-mode-hook nil
  "*List of functions to call when Cfengine mode is invoked.
This is a good place to add specific bindings.")


;;;-------------
;;;  functions
;;;-------------

(defun cfengine-xemacs ()
  (or (string-match "Lucid"  emacs-version)
      (string-match "XEmacs" emacs-version)))

(defun cfengine-create-syntax-table ()
  "Create the syntax table for Cfengine mode."
  (setq cfengine-mode-syntax-table (make-syntax-table (standard-syntax-table)))
  (set-syntax-table cfengine-mode-syntax-table)

  ;; string
  ;(modify-syntax-entry ?\" "\"" cfengine-mode-syntax-table)
  ;(modify-syntax-entry ?\' "\"" cfengine-mode-syntax-table)
  ;(modify-syntax-entry ?\` "\"" cfengine-mode-syntax-table)
  (modify-syntax-entry ?\" "." cfengine-mode-syntax-table)
  (modify-syntax-entry ?\' "." cfengine-mode-syntax-table)
  (modify-syntax-entry ?\` "." cfengine-mode-syntax-table)

  ;; comment
  (modify-syntax-entry ?#  "<"  cfengine-mode-syntax-table)
  (modify-syntax-entry ?\f  ">" cfengine-mode-syntax-table)
  (modify-syntax-entry ?\n  ">" cfengine-mode-syntax-table)

  (modify-syntax-entry ?%  "."  cfengine-mode-syntax-table)
  (modify-syntax-entry ?:  "."  cfengine-mode-syntax-table)
  (modify-syntax-entry ?\; "."  cfengine-mode-syntax-table)
  (modify-syntax-entry ?&  "."  cfengine-mode-syntax-table)
  (modify-syntax-entry ?\|  "." cfengine-mode-syntax-table)
  (modify-syntax-entry ?+  "."  cfengine-mode-syntax-table)
  (modify-syntax-entry ?*  "." cfengine-mode-syntax-table)
  (modify-syntax-entry ?/  "." cfengine-mode-syntax-table)
  (modify-syntax-entry ?=  "." cfengine-mode-syntax-table)
  (modify-syntax-entry ?<  "." cfengine-mode-syntax-table)
  (modify-syntax-entry ?>  "." cfengine-mode-syntax-table)
  (modify-syntax-entry ?$ "."  cfengine-mode-syntax-table)
  (modify-syntax-entry ?. "."  cfengine-mode-syntax-table)
  (modify-syntax-entry ?\\ "." cfengine-mode-syntax-table)
  (modify-syntax-entry ?-  "." cfengine-mode-syntax-table)
  ;; define what belongs in symbols
  (modify-syntax-entry ?_ "_" cfengine-mode-syntax-table)
  ;; define parentheses to match
  (modify-syntax-entry ?\( "()" cfengine-mode-syntax-table)
  (modify-syntax-entry ?\) ")(" cfengine-mode-syntax-table)
  )


;;;###autoload
(defun cfengine-mode ()
  "Cfengine mode is the major mode for editing Cfengine code.

Bindings are as follows:

 Indent line                                          '\\[cfengine-tab]'
 Indent line, insert newline and indent the new line. '\\[newline-and-indent]'

Comments are handled using standard Emacs conventions, including:
 Start a comment                                      '\\[indent-for-comment]'
 Comment region                                       '\\[comment-region]'
 Uncomment region                                     '\\[cfengine-uncomment-region]'
 Continue comment on next line                        '\\[indent-new-comment-line]'
"

  (interactive)
  (kill-all-local-variables)

  (make-local-variable 'require-final-newline)
  (setq require-final-newline t)

  (make-local-variable 'comment-start)
  (setq comment-start "# ")

  ;; comment end must be set because it may hold a wrong value if
  ;; this buffer had been in another mode before. RE
  (make-local-variable 'comment-end)
  (setq comment-end "")

  (make-local-variable 'comment-start-skip) ;; used by autofill
  (setq comment-start-skip "#+[ \t]*")

  (make-local-variable 'indent-line-function)
  (setq indent-line-function 'cfengine-indent-current-function)

  (make-local-variable 'fill-column)
  (setq fill-column 75)

  (make-local-variable 'comment-column)
  (setq comment-column 40)

  (make-local-variable 'parse-sexp-ignore-comments)
  (setq parse-sexp-ignore-comments t)

  (make-local-variable 'case-fold-search)
  (setq case-fold-search t)

  (make-local-variable 'outline-regexp)
  (setq outline-regexp "[^\n\^M]")
  (make-local-variable 'outline-level)
  (setq outline-level 'cfengine-outline-level)

  (make-local-variable 'fill-paragraph-function)
  (setq fill-paragraph-function 'cfengine-fill-comment-paragraph)
  ;;(make-local-variable 'adaptive-fill-regexp)

  (if (cfengine-xemacs) nil ; XEmacs uses properties 
    (make-local-variable 'font-lock-defaults)
    (setq font-lock-defaults
          '((cfengine-font-lock-keywords
             cfengine-font-lock-keywords-1 cfengine-font-lock-keywords-2)
            nil t
            ((?\_ . "w"))
            beginning-of-line
            )))

  (setq major-mode 'cfengine-mode)
  (setq mode-name "Cfengine")

  (use-local-map cfengine-mode-map)

  (if cfengine-mode-syntax-table
      (set-syntax-table cfengine-mode-syntax-table)
    (cfengine-create-syntax-table))

  ;; add menu 'Cfengine' to the menu bar
  ;  (cfengine-add-cfengine-menu)

  (run-hooks 'cfengine-mode-hook))


;;;----------------------;;;
;;; Behaviour Of TAB Key ;;;
;;;----------------------;;;

(defun cfengine-tab ()
  "Do indenting or tabbing according to `cfengine-tab-policy'."
  (interactive)
  (cond (1 (cfengine-tab-hard))
        ((eq cfengine-tab-policy 'indent-auto) (cfengine-indent-current))
        ))

(defun cfengine-untab ()
  "Do dedenting or detabbing."
  (interactive)
  (cfengine-untab-hard))


(defun cfengine-indent-current-function ()
  "Cfengine mode version of the indent-line-function."
  (interactive "*")
  (let ((starting-point (point-marker)))
    (cfengine-beginning-of-line)
    (cfengine-tab)
    (if (< (point) starting-point)
        (goto-char starting-point))
    (set-marker starting-point nil)
    ))


(defun cfengine-tab-hard ()
  "Indent current line to next tab stop."
  (interactive)
  (save-excursion
    (beginning-of-line)
    (insert-char ?  cfengine-indent))
  (if (save-excursion (= (point) (progn (beginning-of-line) (point))))
      (forward-char cfengine-indent)))


(defun cfengine-untab-hard ()
  "indent current line to previous tab stop."
  (interactive)
  (let  ((bol (save-excursion (progn (beginning-of-line) (point))))
        (eol (save-excursion (progn (end-of-line) (point)))))
    (indent-rigidly bol eol  (- 0 cfengine-indent))))



;;;---------------;;;
;;; Miscellaneous ;;;
;;;---------------;;;

(defun cfengine-uncomment-region (beg end)
  "delete `comment-start' at the beginning of a line in the region."
  (interactive "r")
  (comment-region beg end -1))



;;;-----------------------
;;; define keymap for Cfengine
;;;-----------------------

(if (not cfengine-mode-map)
    (progn
      (setq cfengine-mode-map (make-sparse-keymap))

      ;; Indentation and Formatting
      (define-key cfengine-mode-map "\C-j"     'cfengine-indent-newline-indent)
      (define-key cfengine-mode-map "\t"       'cfengine-tab)
      (if (cfengine-xemacs)
	  (define-key cfengine-mode-map '(shift tab)    'cfengine-untab)
	(define-key cfengine-mode-map [S-tab]    'cfengine-untab))
;      (define-key cfengine-mode-map "\M-\C-e"  'cfengine-next-procedure)
;      (define-key cfengine-mode-map "\M-\C-a"  'cfengine-previous-procedure)
;      (define-key cfengine-mode-map "\C-c\C-a" 'cfengine-move-to-start)
;      (define-key cfengine-mode-map "\C-c\C-e" 'cfengine-move-to-end)


      (define-key cfengine-mode-map "\177"     'backward-delete-char-untabify)

      ;; Use predefined function of emacs19 for comments (RE)
      (define-key cfengine-mode-map "\C-c;"    'comment-region)
      (define-key cfengine-mode-map "\C-c:"    'cfengine-uncomment-region)

      ))


;;;-------------------
;;; define menu 'Cfengine'
;;;-------------------

(require 'easymenu)

(defun cfengine-add-cfengine-menu ()
  "Adds the menu 'Cfengine' to the menu bar in Cfengine mode."
  (easy-menu-define cfengine-mode-menu cfengine-mode-map "Menu keymap for Cfengine mode."
                    '("Cfengine"
;                      ["Next Package" cfengine-next-package t]
;                      ["Previous Package" cfengine-previous-package t]
;                      ["Next Procedure" cfengine-next-procedure t]
;                      ["Previous Procedure" cfengine-previous-procedure t]
;                      ["Goto Start" cfengine-move-to-start t]
;                      ["Goto End" cfengine-move-to-end t]
;                      ["------------------" nil nil]
                      ["Indent Current Line (TAB)"
                       cfengine-indent-current-function t]
                      ))
  (if (cfengine-xemacs)
      (progn
        (easy-menu-add cfengine-mode-menu)
        (setq mode-popup-menu (cons "Cfengine mode" cfengine-mode-menu)))))



;;;---------------------------------------------------
;;; support for font-lock
;;;---------------------------------------------------

(defconst cfengine-font-lock-keywords-1
  (list
   ;; actions
   (list "^[ \t]*\\([a-zA-Z0-9]+\\):[^:]" '(1 font-lock-keyword-face) )
   )
  "Subdued level highlighting for Cfengine mode.")

(defconst cfengine-font-lock-keywords-2
  (append cfengine-font-lock-keywords-1
   (list
    ;;
    ;; classes = alphanum or ().|!
    '("^[ \t]*\\([a-zA-Z0-9_\\(\\)\\.\\|\\!]+\\)::" (1 font-lock-function-name-face))
    ;;
    ;; variables
    '("\\$[{(]\\([a-zA-Z0-9_]+\\)[)}]" (1 font-lock-variable-name-face))
    ))
  "Gaudy level highlighting for Cfengine mode.")

(defvar cfengine-font-lock-keywords cfengine-font-lock-keywords-2
  "Default expressions to highlight in Cfengine mode.")


;; set font-lock properties for XEmacs
(if (cfengine-xemacs)
    (put 'cfengine-mode 'font-lock-defaults
         '(cfengine-font-lock-keywords
           nil t ((?\_ . "w")) beginning-of-line)))

;;;
;;; support for outline
;;;

;; used by outline-minor-mode
(defun cfengine-outline-level ()
  (save-excursion
    (skip-chars-forward "\t ")
    (current-column)))


;;; provide ourself

(provide 'cfengine-mode)

;;; cfengine.el ends here



