#lang racket
;(require vigracket)
(require 2htdp/image)
(require 2htdp/universe)


(provide create-world)

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
(define (laengeListenElementeEnd liste)
  (= (length liste) 2))

(define (receive w m)
  ;Korrekter zustand?
  (if (and (list? m) (= (length m) 3))
      (cond  [(and (equal? (car m) 'end)
                   
                   (symbol? (car m))
                   (list? (second m))
                   (= (length (second m)) 4)
                   (andmap laengeListenElementeEnd (second m)))
              m 
              
              ]
             
             [(and (symbol? (car m))
                   (list? (second m))
                   (= (length (second m)) 8)
                   (andmap laengeListenElemente (second m)))
              m
              
              
              
              ]
             
             [else w]);; else fall macht gar nichts
      w
      
      )
  )

;;Zeichnen einer Welt
;;Hilfsfunktionen


(define field (square 50 'outline 'black))
;(define r1 (beside field field field field field field field field))
;; Nimmt die Liste aus dem Zustand, welche die einzelnen Spielfelder darstellt und ruft an jeder Liste in dieser Liste
;; eine Funktion auf, die auf jedes Feld in der Liste etwas hineinsetzt bzw. drueberlegt. Die Variable xs wird also spaeter
;; mit der Liste eines Zustandes belegt.
(define (text->kugel xs)
  (map doppelmap2 xs))
(define (doppelmap2 xs)
  (map symbol->bild xs))
(define (symbol->bild s)
  (cond 
    [(equal? s 'blau) (circle 24 'solid "blue")]
    [(equal? s 'gruen) (circle 24 'solid "green")]
    [(equal? s 'rot) (circle 24 'solid "red")]
    [(equal? s 'gelb) (circle 24 'solid "yellow")]
    ;; elsefall für die leeren Felder
    [else (circle 24 'solid "white")]))
(define (kugel->spielfeld xs)
  (map doppelmap xs))
(define (doppelmap xs)
  (map (lambda (c)   
         (overlay c field)) xs))    
;(overlay (above  r1 r1 r1 r1 r1 r1 r1 r1)
;        (square 160 'solid 'gray))
(define (analyseSpielfarbe f)
  (cond
    [(equal? f 'blau) (circle 10 'solid "blue")]
    [(equal? f 'gruen) (circle 10 'solid "green")]
    [(equal? f 'rot) (circle 10 'solid "red")]
    [(equal? f 'gelb) (circle 10 'solid "yellow")]
    [else (circle 10 'solid "white")]))


(define new_game_text (above(text "Fuer ein neues Spiel" 16 'black)
                            (text "bitte klicken" 16 'black)))

(define (zeigeEndspielstand Endzustandsliste)
  (let ([Spieler1 (assoc 'blau (second Endzustandsliste))]
        [Spieler2 (assoc 'gruen (second Endzustandsliste))]
        [Spieler3 (assoc 'rot (second Endzustandsliste))]
        [Spieler4 (assoc 'gelb (second Endzustandsliste))])
    (above (text "Punkte der Spieler:" 16 'black)
           (beside (text "Blau: " 16 'blau) (text (number->string (second Spieler1)) 16 'blau))
           (beside (text "Grün: " 16 'gruen) (text (number->string (second Spieler2)) 16 'gruen))
           (beside (text "Rot: " 16 'rot) (text (number->string (second Spieler3)) 16 'rot))
           (beside (text "Gelb: " 16 'gelb) (text (number->string (second Spieler4)) 16 'gelb)))))

;;Zustandsliste End : ((Farbe1 Punkte1) (Farbe2 Punkte2) (Farbe3.....))   /assoc
;; Eigentliches Zeichnen
(define (draw name)
  
  (lambda (w)
    (cond;;End
      [(equal? (car w) 'end)
       (above 
        (text "Spielende" 22 'darkgreen)
        (zeigeEndspielstand w))]
      ;;Eval
      [(equal? (car w) 'eval)
       (let ([spielbrett (kugel->spielfeld(text->kugel (second w)))]
             [spielfarbe (analyseSpielfarbe (last w))])
         (above 
          
          (overlay (foldr beside empty-image (first spielbrett)) (rectangle 400 50 'solid 'DimGray))
          (overlay (foldr beside empty-image (second spielbrett)) (rectangle 400 50 'solid 'DimGray))
          (overlay (foldr beside empty-image (third spielbrett)) (rectangle 400 50 'solid 'DimGray))
          (overlay (foldr beside empty-image (fourth spielbrett))(rectangle 400 50 'solid 'DimGray))
          (overlay (foldr beside empty-image (fifth spielbrett)) (rectangle 400 50 'solid 'DimGray))
          (overlay (foldr beside empty-image (sixth spielbrett)) (rectangle 400 50 'solid 'DimGray))
          (overlay (foldr beside empty-image (seventh spielbrett)) (rectangle 400 50 'solid 'DimGray))
          (overlay (foldr beside empty-image (eighth spielbrett)) (rectangle 400 50 'solid 'DimGray))
          (rectangle 400 50 'solid 'white)
          spielfarbe
          (text "Klicke um die Auswertung zu starten" 16 'DarkTurquoise)))]
      ;;Sonst wird normal weiter gespielt
      [else
       (let ([spielbrett (kugel->spielfeld(text->kugel (second w)))]
             [spielfarbe (analyseSpielfarbe (last w))])
         (above 
          
          (overlay (foldr beside empty-image (first spielbrett)) (rectangle 400 50 'solid 'DimGray))
          (overlay (foldr beside empty-image (second spielbrett)) (rectangle 400 50 'solid 'DimGray))
          (overlay (foldr beside empty-image (third spielbrett)) (rectangle 400 50 'solid 'DimGray))
          (overlay (foldr beside empty-image (fourth spielbrett))(rectangle 400 50 'solid 'DimGray))
          (overlay (foldr beside empty-image (fifth spielbrett)) (rectangle 400 50 'solid 'DimGray))
          (overlay (foldr beside empty-image (sixth spielbrett)) (rectangle 400 50 'solid 'DimGray))
          (overlay (foldr beside empty-image (seventh spielbrett)) (rectangle 400 50 'solid 'DimGray))
          (overlay (foldr beside empty-image (eighth spielbrett)) (rectangle 400 50 'solid 'DimGray))
          (rectangle 400 50 'solid 'white)
          spielfarbe
          
          (if (equal? (car w) 'wait)
              (text "warte auf Gegner..." 16 'Firebrick)
              (text "bitte Zelle markieren!" 16 'darkgreen)))
         )])))

;;Maus-Interaktionen
(define (handle-mouse name)
  (lambda (w x_pos y_pos mouse_event)
    (if(mouse=? mouse_event "button-up")
       (cond 
         [(equal? (car w) 'eval) (make-package w (list 'eval))]
         
         [else (let* ((column (quotient x_pos 50))
                      (row    (quotient y_pos 50))
                      )
                 (if #t ;;Feldkoordinaten gültig 
                     
                     (make-package w (list 'setze (list column row)))
                     w))])
       
       w)))

(define (create-world Name Server)
  (big-bang WORLD0
            (on-receive receive)
            (to-draw (draw Name))
            (on-mouse (handle-mouse Name))
            (name Name)
            (register Server)
            )  
  )

;;Macht test Welten auf
;(launch-many-worlds 
; (create-world "Spieler1" LOCALHOST)
; (create-world "Spieler2" LOCALHOST)
; (create-world "Spieler3" LOCALHOST)
; (create-world "Spieler4" LOCALHOST ) )    




