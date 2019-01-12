#lang racket
(require vigracket)         
         
;(resizeimage img 500 500 0)

;(save-image img "/Volumes/students/home/3meyer/BVP/BilderRolit/Klein/NAME")
;(load-image  "/Volumes/students/home/3meyer/BVP/BilderRolit/Groß/NAME")

(define (load)
  (let(
      [anzahl 7]
    )
    (build-list anzahl einzelndLaden)
    )
  )

(define (einzelndLaden n)
  (let*(
      [startnummer 0723]
      [nummer (+ startnummer n)]
      [pfad (string-append "/Volumes/students/home/3meyer/BVP/BilderRolit/Take2/Groß/IMG_0" (number->string nummer) ".jpg")]
      
      )
    (display n)
    (load-image pfad)
    
    )
  
  
  )

(define (resize_all)
(map speichern (load) (build-list 7 (lambda (x) x) )
     )
 
 )

(define (speichern bild n)
           (let*(
      [startnummer 0723]
      [nummer (+ startnummer n)]
      [pfad (string-append "/Volumes/students/home/3meyer/BVP/BilderRolit/Take2/Klein/IMG_0" (number->string nummer) ".jpg")]
      )
             ;(if (> n 6); Bilder haben ein anderes Format
             ;(save-image (resizeimage bild 464 696 0) pfad)
             (save-image (resizeimage bild 696 464 0) pfad)
             ;)
 )
           )
  