// Gramatica tree-sitter de PAED, para el resaltado de Helix.
//
// Helix NO usa JSON para colorear: usa tree-sitter. Hacen falta tres cosas y
// este archivo es la primera:
//
//   1. grammar.js          la gramatica (esto)
//   2. src/parser.c        el parser en C que sale de generar la gramatica
//   3. queries/highlights.scm   que color le toca a cada cosa
//
// Las palabras clave NO se escriben aca: se leen de data/sintaxis.json, que es
// la fuente unica del lenguaje. Agregar una keyword ahi y regenerar alcanza
// para que Helix la pinte, igual que ya pasa con Neovim y con VSCode.
//
// Esta gramatica es LEXICA a proposito: reconoce tokens (palabras clave,
// textos, numeros, comentarios) y la forma de la cabecera, pero no valida la
// estructura del programa. Para colorear alcanza, y el que valida de verdad es
// el parser en C de plugins/ide/. Dos parsers completos que hay que mantener
// sincronizados serian dos fuentes de verdad.

const sintaxis = require("../data/sintaxis.json");

// Devuelve las palabras de una categoria de sintaxis.json.
function palabras(nombre) {
  const cat = (sintaxis.categorias || []).find((c) => c.nombre === nombre);
  return (cat && cat.palabras) || [];
}

// PAED no distingue mayusculas en las palabras clave, asi que 'MIENTRAS',
// 'mientras' y 'MiEnTrAs' son la misma. tree-sitter no tiene una bandera de
// "ignorar mayusculas", asi que cada letra se vuelve una clase: [Mm][Ii]...
function sinMayusculas(palabra) {
  return new RegExp(
    palabra
      .split("")
      .map((c) => {
        const may = c.toUpperCase();
        const min = c.toLowerCase();
        // Los que no son letras (el '_' de FIN_SI) van tal cual.
        return may === min ? c.replace(/[.*+?^${}()|[\]\\]/g, "\\$&") : `[${may}${min}]`;
      })
      .join("")
  );
}

// Una alternativa con las palabras de una categoria.
// Se ordenan de mas larga a mas corta para que 'FIN_SI' gane sobre 'SI': si
// ganara la corta, el resaltado cortaria la palabra por la mitad.
//
// `excluir` saca palabras que ya maneja otra regla. ACCION y ES salen de aca
// porque los usa cabecera_accion: si estuvieran en los dos lados, tree-sitter
// no sabria cual elegir al ver un ACCION y la generacion falla con
// "Unresolved conflict".
//
// Ademas se descartan las palabras que YA se llevo una categoria anterior. En
// sintaxis.json hay repetidas (VENTANA esta en 'tipos' y en 'secuencias'), y
// dos reglas que aceptan el mismo token son otro conflicto irresoluble. Gana
// la categoria que se declara primero, asi que el ORDEN de las llamadas de
// abajo es el que decide de que color queda cada palabra repetida.
const yaTomadas = new Set();

// Los operadores que se escriben con simbolos y no con letras. Salen de la
// categoria 'operadores' porque van a otro color.
const SIMBOLOS = ["*", "/", "+", "-", "**", ":=", "=", "<>", "<", ">", "<=", ">="];

// V y F son los literales booleanos, pero tambien son nombres de variable
// perfectamente validos: la `v` suelta de tests/00-func.paed es una VARIABLE,
// no el booleano verdadero. Como una sola letra, chocan con cualquier programa
// que use nombres cortos.
//
// Mismo problema que 'N' en tipos, y se resuelve igual: por CONTEXTO. V y F
// valen como booleano solo en posicion de VALOR — pegados a lo que los
// precede, `bandera := V` o `bandera = F`. En cualquier otro lugar del
// programa son un identificador comun. Por eso salen de la categoria
// 'booleanos' y viven solo dentro de la regla _valor_booleano.
//
// El token lleva prec(1) para ganarle a identificador cuando SI es un valor:
// las dos reglas matchean una sola letra, asi que sin la precedencia empatan.
// Con nombres mas largos no hay pelea, porque gana el match mas largo:
// en `x := valor`, identificador consume las cinco letras.
const BOOLEANO_VF = token(prec(1, choice(sinMayusculas("V"), sinMayusculas("F"))));

function categoria(nombre, excluir = []) {
  const fuera = excluir.map((e) => e.toUpperCase());
  const ps = palabras(nombre)
    .filter((p) => !fuera.includes(p.toUpperCase()))
    .filter((p) => !yaTomadas.has(p.toUpperCase()))
    .slice()
    .sort((a, b) => b.length - a.length);

  ps.forEach((p) => yaTomadas.add(p.toUpperCase()));

  if (ps.length === 0)
    throw new Error(`la categoria '${nombre}' quedo vacia: todas sus palabras ya estaban tomadas`);

  // token(prec(1, ...)) es lo que hace que las palabras clave GANEN contra la
  // regla de identificador. Sin esto, 'MIENTRAS' matchea igual de largo con
  // /[A-Za-z_][A-Za-z0-9_]*/ y tree-sitter lo toma como un nombre de variable:
  // el archivo entero queda de un solo color menos ACCION y ES.
  return token(prec(1, choice(...ps.map(sinMayusculas))));
}

// Prioridad de las palabras repetidas. Se resuelven ACA, antes de armar las
// reglas, porque el objeto de reglas de abajo no garantiza orden de evaluacion.
// 'tipos' va antes que 'secuencias', asi VENTANA se pinta como tipo: en
// `recorrido: VENTANA DE CARACTER` esta declarando un tipo, no llamando a nada.
const KEYWORDS = {
  condicionales:     categoria("condicionales"),
  bucles:            categoria("bucles"),
  estructura:        categoria("estructura", ["ACCION", "ES"]),
  // AN y N salen de aca: son tipos SOLO cuando llevan parentesis — AN(20),
  // N(5) — y sueltas son nombres de variable perfectamente validos. Sin esta
  // separacion, la variable `N` del ejercicio de inflacion se pinta como si
  // fuera un tipo, que es mentira. PAED.md 10.3: "el parentesis pertenece al
  // TIPO, no a una invocacion".
  tipos:             categoria("tipos", ["AN", "N"]),  // ver tipo_dimensionado
  secuencias:        categoria("secuencias"),
  funciones_builtin: categoria("funciones_builtin"),
  // V y F salen de aca y los maneja _valor_booleano; ver BOOLEANO_VF arriba.
  // HV se queda: no es una sola letra y nadie llama HV a una variable.
  booleanos:         categoria("booleanos", ["V", "F"]),
  // La categoria 'operadores' de sintaxis.json mezcla PALABRAS (Y, O, NO, MOD,
  // DIV) con SIMBOLOS (+, -, :=, =). Son dos cosas distintas para el color: las
  // palabras se pintan como `and`/`or` de Python, los simbolos como los de C.
  // Aca se toman solo las palabras; los simbolos los maneja la regla `operador`.
  operadores:        categoria("operadores", SIMBOLOS),
  salida:            categoria("salida"),
  entrada:           categoria("entrada"),
};

module.exports = grammar({
  name: "paed",

  // `word` le dice a tree-sitter cual es la forma de una palabra del lenguaje.
  // Con esto las palabras clave solo matchean PALABRAS ENTERAS: sin `word`, la
  // 'Y' del operador matchea la Y de adentro de 'MAYOR', y 'V' y 'F' pintan
  // media pantalla de booleanos.
  word: ($) => $.identificador,

  extras: ($) => [/\s/, $.comentario],

  rules: {
    source_file: ($) => repeat($._cosa),

    _cosa: ($) =>
      choice(
        $.cabecera_accion,
        $.tipo_dimensionado,
        $.condicional,
        $.bucle,
        $.estructura,
        $.tipo,
        $.entrada_salida,
        $.funcion_builtin,
        $.secuencia,
        $._valor_booleano,
        $.booleano,
        $.operador_palabra,
        $.texto,
        $.caracter,
        $.numero,
        $.asignacion,
        $.operador,
        $.puntuacion,
        $.identificador
      ),

    // `ACCION <nombre> ES` — el nombre se marca aparte para poder pintarlo
    // como el nombre de una funcion en C, y no como una variable mas.
    cabecera_accion: ($) =>
      seq(
        field("palabra", alias(sinMayusculas("ACCION"), $.estructura)),
        field("nombre", $.identificador),
        field("es", alias(sinMayusculas("ES"), $.estructura))
      ),

    // AN(20): el tipo se lo lleva el parentesis. Sin el, es una variable
    // comun y la pinta la regla `identificador`.
    //
    // 'N' NO entra aca aunque la catedra defina el tipo N(5). Motivo: en el
    // corpus hay 11 usos de `AN(` y CERO de `N(`, mientras que 'N' como nombre
    // de variable si aparece (el anio en el ejercicio de inflacion). Tratarla
    // como tipo pinta de otro color una variable comun, que es peor que no
    // colorear un tipo que nadie escribe. Si algun dia aparece un N(5), se
    // vera como variable y habra que revisar esta decision.
    tipo_dimensionado: ($) =>
      seq(alias(sinMayusculas("AN"), $.tipo), "(", $.numero, ")"),

    condicional:      (_) => KEYWORDS.condicionales,
    bucle:            (_) => KEYWORDS.bucles,
    estructura:       (_) => KEYWORDS.estructura,
    tipo:             (_) => KEYWORDS.tipos,
    funcion_builtin:  (_) => KEYWORDS.funciones_builtin,
    secuencia:        (_) => KEYWORDS.secuencias,
    booleano:         (_) => KEYWORDS.booleanos,

    // El unico lugar donde V y F son booleanos: pegados a := o a un operador.
    // La regla arranca con el operador, no con la letra, asi que en `AMBIENTE
    // v` el token booleano ni siquiera es valido y gana identificador.
    //
    // Va con guion bajo adelante para que sea una regla OCULTA: no agrega un
    // nodo `valor_booleano` al arbol, sus hijos cuelgan del padre. Asi el
    // arbol queda igual de plano que antes y highlights.scm sigue pintando
    // (booleano) sin tocar una linea.
    _valor_booleano: ($) =>
      seq(choice($.asignacion, $.operador), alias(BOOLEANO_VF, $.booleano)),
    operador_palabra: (_) => KEYWORDS.operadores,

    entrada_salida: (_) => choice(KEYWORDS.salida, KEYWORDS.entrada),  // ESCRIBIR / LEER

    // Comentario de linea. PAED usa //, no #.
    //
    // prec 2, por encima de las palabras clave: sin esto, una keyword adentro
    // de un comentario le gana al comentario entero y el texto queda pintado
    // como codigo.
    comentario: (_) => token(prec(2, seq("//", /[^\n]*/))),

    // La catedra escribe los textos con comilla simple (AED_2021_UnI.pdf:10);
    // Frankly acepta ademas la doble. Se aceptan las dos y se pintan igual.
    // Mismo motivo que el comentario: lo que esta entre comillas es texto,
    // aunque adentro diga MIENTRAS.
    texto:    (_) => token(prec(2, choice(seq('"', /[^"\n]*/, '"'), seq("'", /[^'\n]*/, "'")))),
    caracter: (_) => token(prec(3, seq("'", /[^'\n]/, "'"))),

    numero: (_) => /[0-9]+(\.[0-9]+)?/,

    // := es la asignacion; se separa del resto para poder darle otro color,
    // como Helix hace con '=' en C.
    asignacion: (_) => ":=",

    operador: (_) => choice("**", "<>", "<=", ">=", "==", "+", "-", "*", "/", "<", ">", "="),

    puntuacion: (_) => choice(";", ",", ":", "(", ")", "[", "]", ".", ".."),

    identificador: (_) => /[A-Za-z_][A-Za-z0-9_]*/,
  },
});
