#lang racket
(require 2htdp/universe)
(require 2htdp/image)
(require "Spiellogik.rkt")

(provide create-BestTurn-KI-world)


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




;Hauptlogik:
;finde Zug, bei dem am meisten kugeln umgedreht werden (bei gleichstand den, der in der liste weiter vorne ist)

;Liste aller möglichen züge:
(define alleZüge
  (map cons (list 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2 3 3 3 3 3 3 3 3 4 4 4 4 4 4 4 4 5 5 5 5 5 5 5 5 6 6 6 6 6 6 6 6 7 7 7 7 7 7 7 7) 
       (list 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7))
  )


  
  
(define (ziehe m)
  (let* (
        [moeglich (filter (lambda (x) (gueltig (car x) (cdr x) (second m))) alleZüge)]
        [mitGüte (map cons (map (curryr zuggüte m) moeglich) moeglich)]; Erzeugt eine Liste mit zuordnung Güte Zug
;Maximales element der Liste:
[bester (foldl (lambda (elem max)
                 (if (> (car elem) (car max))
                        elem
                        max
                        ))
                  (car mitGüte) (cdr mitGüte))]
[x (car (cdr bester))]
[y (cdr (cdr bester))]


)
(make-package m (list 'setze (list x y)))
    )
  )

;Zuggüte bildet einen Zpielzug (X,Y) auf seine "Güte" ab. 
(define (zuggüte zug m)
(length (umzudrehendeFelder (car zug) (cdr zug) (third m) (second m)))
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
  
  (define (create-BestTurn-KI-world Name Server)
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
  