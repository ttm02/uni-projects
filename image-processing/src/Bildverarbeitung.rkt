#lang racket
(require vigracket)

(require (rename-in 2htdp/image
                    (save-image save-plt-image)
                    (image-width plt-image-width)
                    (image-height plt-image-height)))


;die gesamte BV ist in dieser einen Funktion gekapselt!
; dardurch das zwischendurch Vexctor-set! aufgerufen wird, ist der sequentielle ablauf wichtig,
; dieses Set! fangen wir also in der Closure von spielstandAusBild ein
(provide spielstandAusBild)

(define (spielstandAusBild FILENAME)
  ;Biuld Laden  
  (define Ausgangsbild (load-image FILENAME))
  
  ;Bild Skalieren, damit es für die BV nicht zu groß ist, dardurch steigt die Performance
  
  (define img (if (> (image-width Ausgangsbild) (image-height Ausgangsbild) ); Dann ist das Bild Hochkant
                  (resizeimage Ausgangsbild 696 464 0)
                  (resizeimage Ausgangsbild 464 696 0)
                  )
    )
  
  
  (define red (image->red img))
  (define green (image->green img))
  (define blue (image->blue img))
  
  
  
  (define (maske pixel schwelle) 
    (if (< pixel schwelle)
        0.0
        
        255.0)
    )
  
  
  (define (farbwert-maske bild schwellwert)
    (map (compose car (curry image-map (curryr maske schwellwert)) list) bild)
    )
  
  (define schwellwert_farbe (farbwert-maske img 220.0))
  
  
  ; jetzt wollen wir die Position der 4 Punkte erkennen, dafür gehen wir von obern, unten, links, rechts
  ; durch das bild, nehmen jeweils die ersten roten blauen gelben und grünen punkte die wir finden.
  
  ;(define  RoterPunktX (image-height))
  ;(define RoterPunktY (image-width))
  
  ;jeweils den ersten forbigen Pixel jeder richtung als rand
  ;; BBox: 0 - left, 1 - upper, 2 - right, 3 - lower
  (define (findBBox x y pixel bbox)
    (when (>  (apply + pixel) 0.0)
      (begin
        (when (> x (vector-ref bbox 2))    (vector-set! bbox 2 x))
        (when (< x (vector-ref bbox 0))    (vector-set! bbox 0 x))
        (when (> y (vector-ref bbox 3))    (vector-set! bbox 3 y))
        (when (< y (vector-ref bbox 1))    (vector-set! bbox 1 y)))))
  
  (define bbox1 (vector (image-width schwellwert_farbe) (image-height schwellwert_farbe) 0 0))
  
  
  (image-for-each-pixel (curryr findBBox bbox1)  schwellwert_farbe)
  
  
  
  ;; Bilder zuschneiden (cropping)
  (define (cropimage img ul_x ul_y lr_x lr_y)
    (let ((w (- lr_x ul_x))
          (h (- lr_y ul_y)))
      (racket-image->image (crop ul_x ul_y w h (image->racket-image img)))))
  
  
  (define geschnitten (cropimage img (vector-ref bbox1 0)(vector-ref bbox1 1)(vector-ref bbox1 2)(vector-ref bbox1 3)))
  
  ; Wichtige KONSTANTEN
  (define faktorLoch 0.095)
  (define faktorAbstand 0.0125)
  (define faktorRand 0.07625)
  
  (define bildgroeße (round (/ (+ (image-height geschnitten) (image-width geschnitten)) 2)))
  ; wir vertrauen einfach darauf, das das Bild Quadratisch ist, falls es nicht exakt quadratisch sein Sollte, bilden wir den mittelwert, um geringe schwankungen abzufangen
  
  ;alle Werte werden erst ganz am ende auf ganze Pixel gerundet
  (define loch  (* faktorLoch bildgroeße))
  (define abstand (* faktorAbstand bildgroeße))
  (define rand  (* faktorRand bildgroeße))
  
  (define offset (+ rand (* 0.5 loch)))
  (define schritt (+ abstand loch))
  
  (displayln offset)
  (displayln schritt)
  
  ;jetzt eine Funktion, die in Abhängigkeit von x,y die entsprechende Position auf dem Spielfeld berechnet
  (define (posX->bildkoordX x) (inexact->exact (round (+ offset (* x schritt)))))
  (define (posY->bildkoordY y) (inexact->exact (round (+ offset (* y schritt)))));eigentlich kann man für das da das bild Quadratisch ist auch mit Funktion für X berechnen
  
  ; eine Funktion, die für gegebene x,y den entsprechenden Farbwert im bild berechnet
  (define (farbwert x y)
    (image-ref geschnitten (posX->bildkoordX x) (posY->bildkoordY y) ) 
    ) 
  
  ; eine Funktion, die eine Ganze Bildzeile Berechnet
  ;(8 Spalten) y wird festgehalten, x läuft durch
  ;falls man Spaltenweise suchen möchte: curry statt curryr, dann wird x festgehalten und y läuft durch
  (define (bildzeile zeile) (build-list  8 (compose farbwert->Kugelfarbe (curryr farbwert zeile))))
  
  ;jetzt alle 8 bildzeilen in eine liste tun
  (define (ganzesFeld)
    (build-list 8 bildzeile)  
    )
  
  
  (define (farbwert->Kugelfarbe farbwert)
    (let(
         [delta 20]
         [rot (car farbwert)]
         [gruen (cadr farbwert)]
         [blau (caddr farbwert)]
         )
      ;farbwerte nicht "ausschlagend": Feld ist leer
      (if (and (> 180 rot) (> 180 gruen) (> 180 blau))
          'empty
          ;else
          ;rot+gruen = gelb
          (if (and (> rot blau) (> gruen blau) (< (abs (- rot gruen)) delta))
              'gelb
              ;else
              ;maximaler Farbkanal
              (if (and (> rot blau) (> rot gruen))
                  'rot
                  (if (> gruen blau)
                      'gruen
                      'blau
                      )
                  )
              
              )
          )
      )
    )
  
  
  ;(show-image geschnitten)
  
  (ganzesFeld)
  
  
  
  )

