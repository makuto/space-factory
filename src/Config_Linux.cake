(comptime-define-symbol 'Unix)

;; Uncomment for profiling
;; (comptime-define-symbol 'Profile)

(comptime-cond
 ('Profile
  (add-build-options-global "-O3")
  (add-cakelisp-search-directory "Dependencies/gamelib/src")
  (import "ProfilerAutoInstrument.cake")))
