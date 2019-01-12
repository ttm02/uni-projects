#lang racket
(require 2htdp/universe)
(require "Spiellogik.rkt")


(provide neuesUniversum)

(define Spielerfarben '(rot gruen blau gelb))

; Zustand einer World: 
;'Zustansdssymbol mit aktion, die die Welt machen darf: 'play 'wait 'end....
; Spielfeld .... falls 'end gesetzt ist assotiationsliste Farbe->erreichte Punkte
;Farbe, mit der der Spieler Spielt

;('Zustandssymbol (Spielstandsliste) 'farbe)
;= Liste

;Zum Zustand des Universe:
;Zustand
;Liste der Spieler (mit reihenfolge)
;Spielstand
;Farbzuordnung
;; = Assoziationsliste mit der Zuordnuung World -farbe
;numPlayers
;; = anzahl der Spieler mit denen in diesem universe gespielt wird.

;('zustand (Spielerliste) (Spielstandliste) (farbzuordnung) numPlayers)
;=Liste

;erlaubte Zustände
;'play
;= Welten dürfen spielen
;'wait
;=warte auf weitere Spieler
;'end
;=Spiel beendet

;Quick accessors
(define (status univ)
  (first univ))

(define (spieler univ)
  (second univ))

(define (worlds univ)
  (second univ))

(define (spielfeld univ)
  (third univ))

(define (farbzuordnung univ)
  (fourth univ))

(define (numPlayers univ)
  (fifth univ))



(define (feld x y univ)
  (if (and (< x 8) (< y 8) (>= x 0) (>= y 0))
      (list-ref (list-ref (spielfeld univ) y) x)    
      ;else felder außerhalb des bereichs sind leer
      'empty
      ))


;Suche zu einer world die zugehörige farbe raus
(define (world->farbe world univ)
  (cadr (assoc world (farbzuordnung univ)))
  )




;Registiere neue Welt
(define (neue-welt univ wrld)
  (cond 
    ;;Maximale Anzahl an Spielern erreicht
    ;; --> Weise diese Welt ab
    [(= (length (worlds univ)) (numPlayers univ))
     (make-bundle 
      ;status unverändert lassen
      univ
      ; welt benachichtigen
      (list (make-mail wrld (list 'rejected (spielfeld univ) 'empty)))
      ;und entfernen
      (list wrld)
      )]
    
    ;;Maximale Anzahl an Spielern mit dieser Welt erreicht
    ;; --> Fuege die Welt zu den bekannten hinzu
    ;; --> Starte das Spiel
    [(= (length (worlds univ)) (- (numPlayers univ) 1))
     
     (let* (
            [weltenliste (append (worlds univ) (list wrld))]
            [neueFarbzuordnung (map list weltenliste (take (shuffle Spielerfarben) (numPlayers univ)))]
            )
       
       (make-bundle 
        ;Universumszustand      
        (list 
         'play 
         weltenliste
         (spielfeld univ)
         neueFarbzuordnung
         (numPlayers univ))
        ;mails
        (cons
         (make-mail (car weltenliste) (list 'play (spielfeld univ) (cadr (assoc (car weltenliste) neueFarbzuordnung)  ) ))
         (map (lambda (world) (make-mail world (list 'wait (spielfeld univ) (cadr (assoc world neueFarbzuordnung)) ))) (cdr weltenliste))            
         )
        ;worlds to disconnect
        '()
        )
       )
     ]
    
    ;;Maximale Anzahl an Spielern noch nicht erreicht
    ;; --> Fuege die Welt zu den bekannten hinzu
    [else 
     (make-bundle 
      ;universumszustand
      (list 
       'wait 
       (append (worlds univ) (list wrld))
       (spielfeld univ)
       (farbzuordnung univ)
       (numPlayers univ))
      ;mails           
      (list (make-mail wrld (list 'wait (spielfeld univ) 'empty)))
      ;welten zum disconnecten     
      '())]
    )
  )

(define (tuNichts univ)
  (make-bundle univ '() '()))


;Eingehende Nachricht
(define (handle-msg univ world message)
  
  ;nachricht das Spielende auszuwerten? (diese darf von jeder beteiligten world kommen)
  (if (and (equal? (car message) 'eval) (equal? (status univ) 'eval))
      (auswerten univ)
      ; andernfalls andere Moegliche Nachrichten bearbeiten
      ;ist die sendende Welt an der Reihe?
      ;Und der Zustand des Universe ist Spielen
      (if (and (equal? world (car (worlds univ))) (equal? (status univ) 'play)) 
          ;ja: Behandle die Nachricht
          (cond
            [ (and (equal? (car message) 'setze) (equal? (status univ) 'play))
              (Spielzug (cadr message) (world->farbe world univ) univ)
              ]
            
            
            [else (tuNichts univ)];ungueltige Nachichten nicht behandeln
            )
          ;Nein: Beachte Nachricht nicht weiter:
          
          (tuNichts univ)
          )
      )
  )


; Führe einen Spielzug aus, wenn Erlaubt
;Falls nicht erlaubt: tue nichts (Folgezustand = univ)
; das Ergebnis dieser Funktion ist der Folgezustand des Universums = ein aufruf von make-bundle
(define (Spielzug zug farbe univ)
  (let ([x (first zug)]
        [y (second zug)])
    ;(display zug)
    
    (if (gueltig x y (spielfeld univ))
        ;Zug Ausführen:
        ;Setze die Farbe auf das neue Spielfeld, in dem bereits alles nötige umgedreht wurde
        (NachGueltigemZug (setzeFeld x y 
                                     ;Drehe alle Umzudrehenden Felder um
                                     (foldl (curryr umsetzen farbe) (spielfeld univ)
                                            (umzudrehendeFelder x y farbe (spielfeld univ)) )
                                     farbe) univ)
        ;else: ungueltig: tue nichts
        (tuNichts univ))
    )
  )



(define (auswerten univ)
  (let 
      ([ausgezaehlt (list               
                     ;blau
                     (list 'blau (zaehlePunkte (spielfeld univ) 'blau))
                     ;rot
                     (list 'rot (zaehlePunkte (spielfeld univ) 'rot))
                     ;gelb
                     (list 'gelb (zaehlePunkte (spielfeld univ) 'gelb))
                     ;gruen
                     (list 'gruen (zaehlePunkte (spielfeld univ) 'gruen))
                     )])
    ;(displayln ausgezaehlt)
    
    (make-bundle
     ;State
     (list 'end (worlds univ) ausgezaehlt (farbzuordnung univ) (numPlayers univ))
     ;mails
     (map (lambda (world) (make-mail world (list 'end ausgezaehlt (world->farbe world univ))) ) (worlds univ))            
     
     ;worlds to disconnect
     '()
     )
    
    )    
  )



;Funktion macht aus einem Gueltigen Spielzug einen neuen Universumszustand und benachichtigt alle Welten
(define (NachGueltigemZug Stand univ)
  (let
      ([neueSpielerreihenfolge (append (cdr (worlds univ)) (list (car (worlds univ)) ))])
    (if (spielende? Stand)
        (make-bundle
         ;State
         (list 'play neueSpielerreihenfolge Stand (farbzuordnung univ) (numPlayers univ))
         ;mails
         (cons
          (make-mail (car neueSpielerreihenfolge) (list 'play Stand (world->farbe (car neueSpielerreihenfolge) univ)))
          (map (lambda (world) (make-mail world (list 'wait Stand (world->farbe world univ))) ) (cdr neueSpielerreihenfolge))             
          )
         ;worlds to disconnect
         '()
         )
        ;Alle felder belegt:
        ;letzter zug: Zustand auf 'eval setzen
        (make-bundle
         ;State
         (list 'eval neueSpielerreihenfolge Stand (farbzuordnung univ) (numPlayers univ))
         ;mails
         
         (map (lambda (world) (make-mail world (list 'eval Stand (world->farbe world univ))) ) neueSpielerreihenfolge)             
         ;worlds to disconnect
         '()
         )
        )
    )
  )

; das eigentliche umdrehen: wird mit foldl auf der liste der umzudrehenden felder aufgerufen
(define (umsetzen koord felder farbe)
  (let (
        [x (car koord)]
        [y (cdr koord)]       
        )
    ;(displayln felder)
    (setzeFeld x y felder farbe)
    
    )
  )



;hilfsfunktion setzt feld xy auf die gegebene Farbe resultat ist neues Spielfeld 
(define (setzeFeld x y felder farbe)
  ;(display felder)
  ;(display (list-ref felder y))
  (append
   (take felder y)
   ;Zeile, in der umgedreht werden soll
   (list (append
          (take (list-ref felder y) x)
          (list farbe)
          (drop (list-ref felder y) (+ 1 x))
          ) 
         )  
   (drop felder (+ 1 y))
   )
  )





(define
  (initial-status)
  (list 'wait '() 'SPIELSTAND '() 'NUMPLAYERS)
  )
;SPIELSTAND soll aus dem BV Modul kommen
;NUMPLAYERS muss der user bestimmen

;universum erzeugen:
;(universe (initial-status)
;          (on-new neue-welt)
;          (on-msg handle-msg)
;          
;          )


;TESTDATEN
;(define teststand
;  '((empty empty empty empty empty empty empty empty)
;    (empty empty empty empty empty empty empty empty)
;    (empty empty gelb gelb gelb empty rot empty)
;    (empty empty gelb gruen blau rot empty empty)
;    (empty gelb empty gruen rot rot empty empty)
;    (empty empty empty empty empty empty empty empty)
;    (empty empty empty empty empty empty empty empty)
;    (empty empty empty empty empty empty empty empty))
;  )

;(define teststand
;  '((empty blau empty empty blau empty empty blau)
;    (empty empty rot empty rot empty rot empty)
;    (empty empty gelb gelb gelb rot rot empty)
;    (empty blau gelb gruen empty rot rot blau)
;    (empty gelb empty gruen rot rot empty empty)
;    (empty empty rot empty rot empty rot empty)
;    (empty blau empty empty blau empty empty blau)
;    (empty empty empty empty empty empty empty empty))
;  )

(define teststand
  '((empty empty empty empty empty empty empty empty)
    (empty empty empty empty empty empty empty empty)
    (empty empty empty empty empty empty empty empty)
    (empty empty empty gruen blau empty empty empty)
    (empty empty empty gelb rot empty empty empty)
    (empty empty empty empty empty empty empty empty)
    (empty empty empty empty empty empty empty empty)
    (empty empty empty empty empty empty empty empty))
  )

(define testuniv (list 'wait '() teststand '() 4))

;testuniversum starten
;(universe testuniv
;          (on-new neue-welt)
;          (on-msg handle-msg)          
;          )

;initialisiere ein universe
(define (neuesUniversum Spielstand Spielerzahl)
  (universe 
   (list 'wait '() Spielstand '() Spielerzahl)
   (on-new neue-welt)
   (on-msg handle-msg)
   )
  )