
(defvar *proc*)

(defun hunt ()
  (dotimes (run 10000)
    (run-program "/usr/bin/rm" `("-rf" "/home/wade/.local/share/rr"))
    (let ((*proc* (run-program "/usr/bin/rr"
			       `("record" "-c" "5" "/home/wade/wcl/bin/wcl") 
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
