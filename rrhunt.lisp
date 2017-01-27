;;; Copyright 2017 Wade Lawrence Hennessey
;;;
;;; Licensed under the Apache License, Version 2.0 (the "License");
;;; you may not use this file except in compliance with the License.
;;; You may obtain a copy of the License at
;;;
;;;   http://www.apache.org/licenses/LICENSE-2.0
;;;
;;; Unless required by applicable law or agreed to in writing, software
;;; distributed under the License is distributed on an "AS IS" BASIS,
;;; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
;;; See the License for the specific language governing permissions and
;;; limitations under the License.

(defvar *proc*)

(defun hunt ()
  (dotimes (run 10000)
    (run-program "/usr/bin/rm" `("-rf" "/home/wade/.local/share/rr"))
    (let ((*proc* (run-program "/usr/bin/rr"
			       `("record" "-c" "250" "/home/wade/rtgc/a") 
			       :output t :wait nil)))
      (print *proc*)
      (terpri)
      (dotimes (time 70)
        (sleep 60)
	(let ((status (process-status *proc*)))
	  (unless (process-alive-p *proc*)
	    (format t "~%*******Program died with ~S!*****~%" status)
	    (return-from hunt 'done))
	  (format t "~S - ~D~%" status time)))
      (process-kill *proc* 9 :pid))))
