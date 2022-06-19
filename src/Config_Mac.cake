(comptime-define-symbol 'MacOS)
(comptime-define-symbol 'Unix)

;; Not supported on Macs yet
(comptime-define-symbol 'No-Data-Bundle)
(add-build-options-global "-DNO_DATA_BUNDLE")

(add-build-options-global "-Wno-parentheses-equality")
