Para executar a atividade eu compilei a aplicação utilizando o comando cmake .. -DGMX_BUILD_OWN_FFTW=ON com as flags -DCMAKE_BUILD_TYPE=Release. Após instalar a aplicação (make && sudo make install) executei o comando "source /usr/local/gromacs/bin/GMXRC" para poder utilizar o comando gmx. Após sua utilização, executei os comandos do GROMACS para preparar a simualação:

# Diretório da simulação
mkdir mo833-gmx-simulation

# Arquivos necessários para configurar simulação.
cd mo833-gmx-simulation
wget https://www.ic.unicamp.br/~edson/disciplinas/mo833/2021-1s/anexos/6LVN.pdb
wget https://www.ic.unicamp.br/~edson/disciplinas/mo833/2021-1s/anexos/ions.mdp

# Criando a topologia do ambiente que irá ser simulado.
gmx pdb2gmx -f 6LVN.pdb -o 6LVN_processed.gro -water spce
gmx pdb2gmx -f 6LVN.pdb -o 6LVN_processed.gro -water spce (opção 15)

# Definindo a “caixa” no qual a molécula, os ions e a água irão estar.
gmx editconf -f 6LVN_processed.gro -o 6LVN_newbox.gro -c -d 1.0 -bt cubic

# Adicionando o solvente (água) na caixa:
gmx solvate -cp 6LVN_newbox.gro -cs spc216.gro -o 6LVN_solv.gro -p topol.top

# Adicionando os íons na caixa:
gmx grompp -f ions.mdp -c 6LVN_solv.gro -p topol.top -o ions.tpr
gmx genion -s ions.tpr -o 6LVN_solv_ions.gro -p topol.top -pname NA -nname CL -neutral (opção 13)

# Gerando a simulação:
gmx grompp -f ions.mdp -c 6LVN_solv_ions.gro -p topol.top -o em.tpr

Para executar a simulação e obter os resultados do perf: 
perf record gmx mdrun -v -deffnm em (Gera o arquivo perf.data que está nessa pasta)
perf report (para analisar os dados do perfilamento)

Para executar a simulação e obter os resultados do valgrind:
valgrind --tool=callgrind gmx mdrun -v -deffnm em (Gera o arquivo callgrind.out.1189)
callgrind_annotate callgrind.out.1189 (para analisar os dados do perfilamento)



