(set-cakelisp-option cakelisp-src-dir "Dependencies/cakelisp/src")
(add-cakelisp-search-directory "Dependencies/gamelib/src")
(add-cakelisp-search-directory "Dependencies/cakelisp/runtime")
(add-cakelisp-search-directory "src")

(import "SDL.cake")

(add-c-search-directory-global "src")

;; Allow including Cakelisp headers
(add-c-search-directory-global "cakelisp_cache/default")

(add-c-build-dependency
 "main.c")

;; Needed so SDL can open DLLs...not sure when it does that, but I'm guessing it is required for
;; interacting with X/Wayland, whatever at the very least
(add-linker-options "-ldl")

(comptime-cond
 ('Windows
  (set-cakelisp-option executable-output "SpaceFactory.exe"))
 (true
  (set-cakelisp-option executable-output "space-factory")))