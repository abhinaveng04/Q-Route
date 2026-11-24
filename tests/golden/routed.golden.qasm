// circuit            : data/circuit.qasm
// coupling           : data/coupling.map
// pipeline_revision  : R2
// logical_qubits     : 5
// physical_qubits    : 5
// inserted_swaps     : 3
// added_cnots        : 9
// cumulative_fidelity: 0.981164
OPENQASM 2.0;
include "qelib1.inc";
qreg p[5];
creg c[5];
h p[0];
rx(0.12) p[0];
ry(0.45) p[2];
rz(pi/4) p[4];
barrier p[0],p[2],p[4],p[1],p[0];
cx p[0],p[1];      // swap-component 1/3 (logical SWAP p0p1)
cx p[1],p[0];      // swap-component 2/3
cx p[0],p[1];      // swap-component 3/3
cx p[1],p[2];      // swap-component 1/3 (logical SWAP p1p2)
cx p[2],p[1];      // swap-component 2/3
cx p[1],p[2];      // swap-component 3/3
cx p[2],p[3];      // routed CX (logical cx q[0],q[4])
cx p[1],p[0];      // routed CX (logical cx q[1],q[3])
cx p[2],p[1];      // swap-component 1/3 (logical SWAP p1p2)
cx p[1],p[2];      // swap-component 2/3
cx p[2],p[1];      // swap-component 3/3
cx p[1],p[0];      // routed CX (logical cx q[0],q[3])
cx p[2],p[3];      // routed CX (logical cx q[1],q[4])
measure p[1] -> c[0];
measure p[2] -> c[1];
measure p[4] -> c[2];
measure p[0] -> c[3];
measure p[3] -> c[4];
