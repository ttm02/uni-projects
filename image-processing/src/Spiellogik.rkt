#lang racket

;stellt wesentliche funktionen der Spiellogik bereit

(provide gueltig)
(provide umzudrehendeFelder)
(provide zaehlePunkte)
(provide spielende?)


(define (feld x y spielfeld)
  (if (and (< x 8) (< y 8) (>= x 0) (>= y 0));Konstante 8x8Matrix angenommen
      (list-ref (list-ref spielfeld y) x)    
      ;else felder außerhalb des bereichs sind leer
      'empty
      ))

; Spielzug auf gueltigkeit überprüfen
; #t wenn spielzug erlaubt ist
; = die Kugel ligt neben einer anderen
(define (gueltig x y spielfeld)
  (and (equal? (feld x y spielfeld) 'empty);leer
       (or ;mindestens 1 angerenzend
        ; oben links
        (not (equal? (feld (- x 1) (- y 1) spielfeld) 'empty))
        ; oben 
        (not (equal? (feld  x  (- y 1) spielfeld) 'empty))
        ; oben rechts
        (not (equal? (feld (+ x 1) (- y 1) spielfeld) 'empty))
        ; links
        (not (equal? (feld (- x 1) y spielfeld) 'empty))
        ; rechts
        (not (equal? (feld (+ x 1) y spielfeld) 'empty))
        ; unten links
        (not (equal? (feld (- x 1) (+ 1 y) spielfeld) 'empty))
        ; unten 
        (not (equal? (feld  x  (+ 1 y) spielfeld) 'empty))
        ; unten rechts
        (not (equal? (feld (+ x 1) (+ 1 y) spielfeld) 'empty))
        )
       )
  )


; richtung als cons zelle x offset y offset
; ergebnis: liste aller Felder, die umgedreht werden solln
; felldkoord als cons Zelle (x . y)
(define (umzudrehendeFelderRekursion richtung x y akku farbe spielfeld)
  (if (or (< x 0) (< y 0) (> x 7) (> y 7) (equal? 'empty (feld x y spielfeld) ) );Konstante 8x8Matrix angenommen
      ;abbruch: entweder leeres Feld gefunden, oder außerhalb des Bereichs
      '()
      ;else
      (if (equal? farbe (feld x y spielfeld))
          akku
          ;else: weiter mit Rekursion
          (umzudrehendeFelderRekursion richtung (+ (car richtung) x) (+ (cdr richtung) y) (cons (cons x y)  akku) farbe spielfeld )
          )
      
      )
  )

(define (umzudrehendeFelder x y farbe spielfeld)
  (let (
        [obenLinks (umzudrehendeFelderRekursion (cons -1 -1) (- x 1) (- y 1) '() farbe spielfeld)]
        [oben (umzudrehendeFelderRekursion (cons 0 -1) x (- y 1) '() farbe spielfeld)]
        [obenRechts (umzudrehendeFelderRekursion (cons +1 -1) (+ 1 x) (- y 1) '() farbe spielfeld)]
        [Links (umzudrehendeFelderRekursion (cons -1 0) (- x 1) y '() farbe spielfeld)]
        [Rechts (umzudrehendeFelderRekursion (cons +1 0) (+ 1 x) y '() farbe spielfeld)]
        [untenLinks (umzudrehendeFelderRekursion (cons -1 +1) (- x 1) (+ 1 y) '() farbe spielfeld)]
        [unten (umzudrehendeFelderRekursion (cons 0 +1) x (+ 1 y) '() farbe spielfeld)]
        [untenRechts (umzudrehendeFelderRekursion (cons +1 +1) (+ 1 x) (+ 1 y) '() farbe spielfeld)]
        )
    (append obenLinks oben obenRechts Links Rechts untenLinks unten untenRechts)
    )
    )


(define (zaehlePunkte stand farbe)
  (foldl 
   (lambda (elem anz)
     (if (equal? elem farbe)
         (+ anz 1)
         anz ;else
         ))
   0
   (flatten stand)
   )
  )

;Kann man das Spiel noch weiter spielen
;wenn minestens ein feld empty ist
(define (spielende? Stand)
  (ormap (curry ormap (curry equal? 'empty) ) Stand)   
  )
