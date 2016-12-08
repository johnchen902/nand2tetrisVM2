/*
 * Compiles HACK machine code to C.
 * Copyright (C) 2016 Pochang Chen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <iostream>
#include <fstream>
#include <cstdint>
#include <bitset>

namespace {
using std::uint16_t;

constexpr uint16_t abit = 1 << 12;
constexpr uint16_t zxbit = 1 << 11;
constexpr uint16_t nxbit = 1 << 10;
constexpr uint16_t zybit = 1 << 9;
constexpr uint16_t nybit = 1 << 8;
constexpr uint16_t fbit = 1 << 7;
constexpr uint16_t nobit = 1 << 6;
constexpr uint16_t d1bit = 1 << 5;
constexpr uint16_t d2bit = 1 << 4;
constexpr uint16_t d3bit = 1 << 3;
constexpr uint16_t j1bit = 1 << 2;
constexpr uint16_t j2bit = 1 << 1;
constexpr uint16_t j3bit = 1 << 0;

unsigned length;
uint16_t code[1 << 16];

void translate_out(std::ostream &out, uint16_t inst) {
    bool a  = inst &  abit;
    bool zx = inst & zxbit;
    bool nx = inst & nxbit;
    bool zy = inst & zybit;
    bool ny = inst & nybit;
    bool f  = inst &  fbit;
    bool no = inst & nobit;
    if(no)
        out << "~(";
    if(nx)
        out << "~";
    out << (zx ? "0" : "d") << " " << (f ? "+" : "&") << " ";
    if(ny)
        out << "~";
    out << (zy ? "0" : a ? "read(a)" : "a");
    if(no)
        out << ")";
}

void translate_inst(std::ostream &out, uint16_t inst) {
    if((inst & (1 << 15)) == 0) {
        // A instruction
        out << "    a = " << inst << ";\n";
        return;
    }
    bool d1 = inst & d1bit;
    bool d2 = inst & d2bit;
    bool d3 = inst & d3bit;
    bool j1 = inst & j1bit;
    bool j2 = inst & j2bit;
    bool j3 = inst & j3bit;
    bool mayass = d1 || d2 || d3;
    bool jmp = j1 && j2 && j3;
    bool mayjmp = j1 || j2 || j3;
    // Assuming C instruction
    if(!mayjmp) {
        if(!mayass) {
            out << "    ;\n";
            return;
        }
        if(!d3) {
            out << "    ";
            if(d1)
                out << "a = ";
            if(d2)
                out << "d = ";
            translate_out(out, inst);
            out << ";\n";
            return;
        }
        if(!d1) {
            out << "    write(a, ";
            if(d2)
                out << "d = ";
            translate_out(out, inst);
            out << ");\n";
            return;
        }
        // d1 && d3, nojump
        out << "    {\n";
        out << "        uint16_t out = ";
        translate_out(out, inst);
        out << ";\n";
        if(d2)
            out << "        d = out;\n";
        out << "        write(a, out);\n";
        out << "        a = out;\n";
        out << "    }\n";
        return;
    } else {
        if(!mayass) {
            if(jmp) {
                out << "    goto *(&&lb0 + lb[a]);\n";
                return;
            }
            out << "    if((int16_t) (";
            translate_out(out, inst);
            out << ") " <<
                (j1 ? j2 ? "<=" : j3 ? "!=" : "<"
                    : j2 ? j3 ? ">=" : "==" : ">") << " 0)\n";
            out << "        goto *(&&lb0 + lb[a]);\n";
            return;
        }
        if(jmp && !d1) {
            if(d3)
                out << "    write(a, ";
            if(d2)
                out << "d = ";
            translate_out(out, inst);
            if(d3)
                out << ")";
            out << ";\n";
            out << "    goto *(&&lb0 + lb[a]);\n";
            return;
        }
    }
    // mayjmp, mayass, (conditional jump or d1)
           out << "    {\n";
           out << "        uint16_t out = ";
               translate_out(out, inst);
               out << ";\n";
           out << "        uint16_t a0 = a;\n";
    if(d1) out << "        a = out;\n";
    if(d2) out << "        d = out;\n";
    if(d3) out << "        write(a0, out);\n";
    if(!jmp) {
           out << "        if((int16_t) out " <<
                   (j1 ? j2 ? "<=" : j3 ? "!=" : "<"
                       : j2 ? j3 ? ">=" : "==" : ">")
                   << " 0)\n    ";
    }
           out << "        goto *(&&lb0 + lb[a0]);\n";
           out << "    }\n";
}

void translate(std::ostream &out) {
    out << "#include <stdint.h>\n"
           "extern void write_screen(uint16_t, uint16_t);\n"
           "extern uint16_t read_key();\n"
           "static uint16_t a, d, mem[1 << 16];\n"
           "static uint16_t read(uint16_t addr) {\n"
           "    if(addr == 0x6000)\n"
           "        return read_key();\n"
           "    return mem[addr];\n"
           "}\n"
           "static void write(uint16_t addr, uint16_t val) {\n"
           "    mem[addr] = val;\n"
           "    if(addr >= 0x4000 && addr < 0x6000)\n"
           "        write_screen(addr, val);\n"
           "}\n"
           "void program() {\n"
           "    static const int lb[] = {\n";
    for(unsigned i = 0; i < length; i++)
        out << "        &&lb" << i << " - &&lb0,\n";
    for(unsigned i = length; i < (1 << 16); i++)
        out << "        &&lbend - &&lb0,\n";
    out << "    };\n";
    for(unsigned i = 0; i < length; i++) {
        out << "lb" << i << ":\n";
        translate_inst(out, code[i]);
    }
    if(length < (1 << 16)) {
        out << "lbend:\n";
        out << "    a = 0;\n";
    }
    out << "    goto lb0;\n";
    out << "}\n";
}
}
int main() {
    std::bitset<16> x;
    while(length < (1 << 16) && std::cin >> x)
        code[length++] = x.to_ulong();
    if(length == 0) {
        std::cerr << "No code supplied" << std::endl;
        return 1;
    }
    std::ofstream out("program.c");
    translate(out);
}

