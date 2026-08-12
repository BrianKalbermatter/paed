#!/bin/bash
# stdlib/matematica.sh — Funciones matemáticas predefinidas del lenguaje .paed
# Uso: sourced por paed. Cada función toma sus args y devuelve el resultado por stdout.

# MOD(a, b) → resto de la división entera  (a mod b)
_paed_MOD()   { echo "scale=0; ($1) % ($2)" | bc -l 2>/dev/null; }

# DIV(a, b) → cociente entero               (a div b)
_paed_DIV()   { echo "scale=0; ($1) / ($2)" | bc -l 2>/dev/null; }

# TRUNC(x)  → parte entera, trunca hacia cero
_paed_TRUNC() { echo "scale=0; ($1) / 1" | bc -l 2>/dev/null; }

# ABSO(x)   → valor absoluto
_paed_ABSO()  { echo "define abs(x) { if (x<0) return -x; return x; } abs($1)" | bc -l 2>/dev/null; }

# REDOND(x) → redondeo al entero más cercano
_paed_REDOND(){ echo "scale=0; ($1 + 0.5) / 1" | bc -l 2>/dev/null; }
