// Python interface to PWS parser
// (C) 2017 Riad S. Wahby <rsw@cs.nyu.edu>

#include <gmp.h>
#include <iostream>
#include <Python.h>
#include <string.h>
#include <string>
#include <vector>

#include "pws_circuit_parser.hh"

using namespace std;

static PyObject *pylibpws_parse_pws(PyObject *self __attribute__((unused)), PyObject *args);
static PyObject *pylibpws_parse_pws_unopt(PyObject *self __attribute__((unused)), PyObject *args);
static PyObject *py_circuitdesc(PWSCircuitParser &parser);
static PyObject *parse_or_optimize(bool optimize, PyObject *args);

// method table and init
static PyMethodDef pylibpws_Methods[] = {
    {"parse_pws", pylibpws_parse_pws, METH_VARARGS, "Parse a PWS file."},
    {"parse_pws_unopt", pylibpws_parse_pws_unopt, METH_VARARGS, "Parse a PWS file (optimizations off)."},
    {NULL, NULL, 0, NULL},
};
PyMODINIT_FUNC initpylibpws(void) {
    (void) Py_InitModule("pylibpws", pylibpws_Methods);
}

// parse a PWS into a format that Python can use.
static PyObject *pylibpws_parse_pws(PyObject *self __attribute__((unused)), PyObject *args) {
    return parse_or_optimize(true, args);
}

// parse a PWS into a format that Python can use.
static PyObject *pylibpws_parse_pws_unopt(PyObject *self __attribute__((unused)), PyObject *args) {
    return parse_or_optimize(false, args);
}

static PyObject *parse_or_optimize(bool optimize, PyObject *args) {
    char *filename;
    char *prime_str;
    if (! PyArg_ParseTuple(args, "ss", &filename, &prime_str)) {
        return NULL;
    }

    mpz_t prime;
    mpz_init_set_str(prime, prime_str, 10);
    PWSCircuitParser parser(prime);
    try {
        if (optimize) {
            parser.optimize(filename);
        } else {
            parser.parse(filename);
        }
    } catch (...) {
        PyErr_SetString(PyExc_IOError, "Error opening/parsing PWS file.");
        return NULL;
    }

    mpz_clear(prime);
    if (parser.circuitDesc.size() == 0) {
        PyErr_SetString(PyExc_ValueError, "PWS has circuit with size 0.");
        return NULL;
    } else {
        return py_circuitdesc(parser);
    }
}

static PyObject *py_circuitdesc(PWSCircuitParser &parser) {
    PyObject *pyckt = PyList_New(parser.circuitDesc.size());

    // build input layer: ints for constants, None for variables
    unsigned inlayer_size = parser.circuitDesc[0].size();
    PyObject *pyInLayer = PyList_New(inlayer_size);
    PyList_SetItem(pyckt, 0, pyInLayer);
    for (unsigned i = 0, j = 0, k = 0; k < inlayer_size; i++, k++) {
        j = k;

        if (i >= parser.inConstants.size()) {
            k = inlayer_size;
        } else {
            k = get<1>(parser.inConstants[i]);
        }

        for (; j < k; j++) {
            Py_INCREF(Py_None);
            PyList_SetItem(pyInLayer, j, Py_None);
        }

        if (k < inlayer_size) {
            string const_string = get<0>(parser.inConstants[i]).c_str();
            char *const_cstr = new char[const_string.length() + 1];
            strncpy(const_cstr, const_string.c_str(), const_string.length());
            const_cstr[const_string.length()] = 0;

            PyList_SetItem(pyInLayer, k, PyLong_FromString(const_cstr, NULL, 0));

            delete []const_cstr;
        }
    }

    // go through each layer and build tuples for each gate
    for (unsigned layNum = 1; layNum < parser.circuitDesc.size(); layNum++) {
        LayerDescription &layer = parser.circuitDesc[layNum];
        unsigned lay_size = layer.size();
        PyObject *pyLayer = PyList_New(lay_size);
        PyList_SetItem(pyckt, layNum, pyLayer);
        for (unsigned gNum = 0; gNum < lay_size; gNum++) {
            GateDescription &gate = layer[gNum];
            PyObject *pyGate = Py_BuildValue("(siiN)", gate.strOpType().c_str(), gate.in1, gate.in2, Py_None);
            if (gate.op == GateDescription::MUX) {
                PyTuple_SetItem(pyGate, 3, PyInt_FromLong(parser.muxGates[gate.pos.layer].at(gate.pos.name)));
            } else {
                Py_INCREF(Py_None);
            }
            PyList_SetItem(pyLayer, gNum, pyGate);
        }
    }

    return pyckt;
}
