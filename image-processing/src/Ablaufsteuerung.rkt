#lang racket
(require racket/gui)
(require 2htdp/universe)


(require "Bildverarbeitung.rkt")

(require "Universe.rkt")
(require "RolitWorld.rkt")
(require "RandomKIworld.rkt")
(require "BestTurnKIworld.rkt")
(require "AdvBestTurnKIworld.rkt")


;Frame erzeugen
(define frame (new frame% [label "Rolit"]))


;anzeigen
(send frame show #t)



;Gui um ein neues Universe zu starten
(define panelUniverse (new vertical-panel% [parent frame] [border 20]))

(new message% [parent panelUniverse]
     [label "Starte ein neues Spiel"]
     )


(define spielerzahl (new slider%
                         (label "Anzahl der Spieler")
                         (parent panelUniverse)
                         (min-value 2)
                         (max-value 4)
                         (init-value 4)))

(define KIzahlLeicht (new slider%
                          (label "davon schwächere KI-Spieler")
                          (parent panelUniverse)
                          (min-value 0)
                          (max-value 3)
                          (init-value 0)))

(define KIzahlMittel (new slider%
                          (label "davon stärkere KI-Spieler")
                          (parent panelUniverse)
                          (min-value 0)
                          (max-value 3)
                          (init-value 0)))

(define KIzahlSchwer (new slider%
                          (label "davon starke KI-Spieler")
                          (parent panelUniverse)
                          (min-value 0)
                          (max-value 3)
                          (init-value 0)))


; Gui um eine neue world zu starten

(define panelWorld (new vertical-panel% [parent frame] [border 20]))

(new message% [parent panelWorld]
     [label "Spiele bei einem Spiel mit"]
     )

(define spielername (new text-field%
                         (label "Spielername:")
                         (parent panelWorld)
                         (init-value "anonym")))

(define serveraddresse (new text-field%
                            (label "Serveradresse:")
                            (parent panelWorld)
                            (init-value "LOCALHOST")))



(define worldButton (new button% [parent panelWorld]
                         [label "Trete Spiel bei"]
                         [callback (lambda (button event)
                                     (create-world (send spielername get-value) (send serveraddresse get-value))  )])
  )




; Logik für den Button:
(define (starteUniverse) 
  
  (let
      ([filename (get-file "Bild mit dem Spielstand auswählen" frame)])
    (if filename
        (let*
            ([spielstand (spielstandAusBild filename)]
             [anzSpieler (send spielerzahl get-value)]
             [anzKILeicht (send KIzahlLeicht get-value)]
             [anzKIMittel (send KIzahlMittel get-value)]
             [anzKISchwer (send KIzahlSchwer get-value)]
             [univ (list (list 'neuesUniversum (list 'quote spielstand) anzSpieler))] ; es soll '(neuesUniversum (quote spielstand) anzSpieler) mit ausgewerteten argumenten (spielstand und anzSpieler sind ausgewertet) rauskommen, sodass ein aufruv fon eval das universum erzeugen kann
             [KiLeicht (build-list anzKILeicht (lambda (n) '(create-Random-KI-world "KI" "LOCALHOST") ))]
             [KiMittel (build-list anzKIMittel (lambda (n) '(create-BestTurn-KI-world "KI" "LOCALHOST") ))]
             [KiSchwer (build-list anzKISchwer (lambda (n) '(create-AdvBestTurn-KI-world "KI" "LOCALHOST") ))]
             [world (list  (list 'create-world (send spielername get-value) "LOCALHOST") )] ; es soll '(list 'create-world (send spielername get-value) "LOCALHOST") mit ausgewerteten argumenten rauskommen, sodass ein aufruf fon eval die World erzeugen kann
             
             )
          
          (if (< (+ anzKILeicht anzKIMittel anzKISchwer) anzSpieler);nicht zu viele KI spieler ausgewählt
          ;(display (append (list 'launch-many-worlds) univ KiLeicht KiMittel world))
          (eval  (append (list 'launch-many-worlds) univ KiLeicht KiMittel KiSchwer world))  
          #f;nichts tun 
          )
                  
          
          )   
        
        ; else
        #f
        )
    )
  )

; die Parameter button und event werden einfach ignoriert





(new button% [parent panelUniverse]
     [label "Wähle Spielstand aus"]
     [callback (lambda (button event)       
                 (starteUniverse))                 
               ])
;Die eigene Addresse raussuchen

(define ip-addresse
  (with-output-to-string (lambda () (system "curl ipecho.net/plain")))
  )

(new message% [parent panelUniverse]
     [label (string-append "Deine Addresse: " ip-addresse)]
     )












