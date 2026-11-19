#include <cstdint>
#include <iostream>
#include "runtime/execution/machine_state.h"
bool pes17_fn_047d5000(pes17::execution::MachineState& state);
int main() {
 constexpr std::uint32_t base=0x10000000u,list=base+0x100u,target=base+0x300u,stack=base+0x800u,oldEbp=0x55667788u;
 pes17::execution::MachineState state(base,0x1000); state.reg(pes17::ir::Register::EAX)=5; state.reg(pes17::ir::Register::ECX)=list; state.reg(pes17::ir::Register::EBP)=stack;
 bool ready=true; ready&=state.memory.write32(list,target); ready&=state.memory.write32(list+4,7); ready&=state.memory.write32(list+8,0); ready&=state.memory.write32(target,0); ready&=state.memory.write32(stack,oldEbp);
 if(!ready||!pes17_fn_047d5000(state)) return 2; auto value=state.memory.read32(target);
 bool pass=value&&*value==0xc&&state.reg(pes17::ir::Register::ECX)==list+8&&state.reg(pes17::ir::Register::EBP)==oldEbp&&state.reg(pes17::ir::Register::ESP)==stack+4;
 std::cout<<"Compiled PES loop comparison: "<<(pass?"PASS":"FAILED")<<'\n'; if(value) std::cout<<"Target=0x"<<std::hex<<*value<<'\n'; std::cout<<"ECX=0x"<<std::hex<<state.reg(pes17::ir::Register::ECX)<<'\n'<<"EBP=0x"<<state.reg(pes17::ir::Register::EBP)<<'\n'<<"ESP=0x"<<state.reg(pes17::ir::Register::ESP)<<'\n'; return pass?0:3;
}
