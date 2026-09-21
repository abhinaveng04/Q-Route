OPENQASM 2.0;
include "qelib1.inc";

qreg q[5];
creg c[5];

h q[0];
rx(0.12) q[0];
ry(0.45) q[1];
rz(pi/4) q[2];

barrier q[0],q[1],q[2],q[3],q[4];

cx q[0],q[4];
cx q[1],q[3];
cx q[0],q[3];
cx q[1],q[4];

measure q[0] -> c[0];
measure q[1] -> c[1];
measure q[2] -> c[2];
measure q[3] -> c[3];
measure q[4] -> c[4];