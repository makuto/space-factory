(set-cakelisp-option use-c-linkage true)
(comptime-define-symbol 'Windows)

(add-build-options-global "/DWINDOWS")

(add-c-build-dependency "DPI.c")
(add-static-link-objects "User32.lib")

;; Debug
;; (add-build-options-global "/Zi")
;; (add-linker-options "/DEBUG")

