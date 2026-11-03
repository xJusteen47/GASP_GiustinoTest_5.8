# Scripts > GSTN > Mostra barra pulsanti: reopens the GSTN button bar (it also opens by itself when Cascadeur starts).
from gstn_scripts import _bar


def name():
    return "GSTN.Mostra barra pulsanti"


def description():
    return "Riapre la barra con i pulsanti GSTN (Segna chiave, Invia, riga di stato)"


def run(scene):
    _bar.show()
