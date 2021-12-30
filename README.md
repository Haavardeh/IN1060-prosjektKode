# IN1060-prosjektKode
Arduino kode fra IN1060 - Bruksorientert design

Programmet styrer et sett med grønne og rød led-pærer koblet til en Arduino. Det er også koblet til en lys-sensor som skal registrere grad av mørke fra omgivelsene. Når lys-sensoren registrerer mørke under et visst punkt, skal Led-pærene skifte fra rødt til grønt lys.

Ettersom lyset fra omgivelsene varierer har vi lagt til en funksjon som skal kalibrere lys- sensoren hvert sekund slik at mørke defineres uavhengig av hvor lyst eller mørkt det er i rommet fra før.
