#lang racket
(require 2htdp/universe)
(require 2htdp/image)
(require "Spiellogik.rkt")

(provide create-Random-KI-world)


;;Zustand 
;; w = (wait (ListeDesSpielFeldes) Spielfarbe)
;;Startzustand:

(define WORLD0 (list 'wait (list (make-list 8 'empty) 
                                 (make-list 8 'empty)
                                 (make-list 8 'empty)
                                 (list 'empty 'empty 'empty 'blau 'gelb 'empty 'empty 'empty)
                                 (list 'empty 'empty 'empty 'rot 'gruen 'empty 'empty 'empty)
                                 (make-list 8 'empty) 
                                 (make-list 8 'empty)
                                 (make-list 8 'empty)) 'empty))
;;Falls ein korrekter Weltzustand empfangen wird
;;   --> setze die Welt auf die empfangene Nachricht
;;sonst: verharre im alten Weltzustand
(define (laengeListenElemente liste)
  (= (length liste) 8)) 

(define (ziehe m)
  (let (
        [x (random 8)]
        [y (random 8)])
    (if (gueltig x y (second m))
        (make-package m (list 'setze (list x y)))
        (ziehe m)
        )
    )
  )

(define (receive w m)
  ;Prüfe ob gueltiger Zustand empfangen wurde
  (if (and (list? m) (= (length m) 3)
           (symbol? (car m))
           (list? (second m))
           (= (length (second m)) 8)
           (andmap laengeListenElemente (second m)))
      ;gueltiger Zustand
      (if (equal? (car m) 'play)
          ;Bei 'play spielen
          (ziehe m)
          ;Wenn nicht dran, warten
          m
          )
          ;kein gueltiger Zustand, keine Aktion
          w)
  )
  
  (define (draw n)
    (text "KI-World" 24 "orange")
    )
  
  (define (create-Random-KI-world Name Server)
   (big-bang WORLD0
            (on-receive receive)
            (to-draw draw)
            ;(on-mouse (handle-mouse Name))
            (name Name)
            (register Server)
            )  
  )

  
  ;;Macht zwei Welten auf
  ;(launch-many-worlds 
  ; (create-world "Spieler1" LOCALHOST)
  ; (create-world "Spieler2" LOCALHOST)
  ; (create-world "Spieler3" LOCALHOST)
  ;)     
  