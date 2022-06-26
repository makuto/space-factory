(set-cakelisp-option use-c-linkage true)
(comptime-define-symbol 'Windows)

;; Debug
(add-build-options-global "/Zi" "/DWINDOWS")
(add-linker-options "/DEBUG")

