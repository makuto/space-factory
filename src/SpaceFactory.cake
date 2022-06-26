(set-cakelisp-option cakelisp-src-dir "Dependencies/cakelisp/src")
(set-cakelisp-option cakelisp-lib-dir "Dependencies/cakelisp/bin")

(add-cakelisp-search-directory "Dependencies/gamelib/src")
(add-cakelisp-search-directory "Dependencies/cakelisp/runtime")
(add-cakelisp-search-directory "src")

(import "SDL.cake" "DataBundle.cake")

(comptime-cond
 ('No-Data-Bundle (ignore))
 (true
  (bundle-file start-tilesheet-bmp end-tilesheet-bmp (unsigned char) "assets/TileSheet.bmp")
  (bundle-file start-logo-bmp end-logo-bmp (unsigned char) "assets/SpaceFactoryLogo.bmp")))

(add-c-search-directory-global "src")

;; Allow including Cakelisp headers
(add-c-search-directory-global "cakelisp_cache/default")

(add-c-build-dependency
 "main.c")

;; Needed so SDL can open DLLs...not sure when it does that, but I'm guessing it is required for
;; interacting with X/Wayland, whatever at the very least
(comptime-cond
 ('Unix
  (add-linker-options "-ldl")))

(comptime-cond
 ('Windows
  (set-cakelisp-option executable-output "SpaceFactory.exe"))
 (true
  (set-cakelisp-option executable-output "space-factory")))


(defun initialize-cakelisp ()
  (data-bundle-load-all-resources))
