Para executar a atividade, executei os seguintes passos (no repositório Distributed-DCGAN):

1. Fiz o download e extraí o dataset utilizado (CIFAR-10):
mkdir cifar10 && cd cifar10
wget --no-check-certificate https://www.cs.toronto.edu/~kriz/cifar-10-python.tar.gz
tar -xvf cifar-10-python.tar.gz
cd ..

2. Configurei a imagem docker utilizada:
docker build -t dist_dcgan .

3. Executei o comando abaixo um total de 12 vezes (3 vezes para cada valor adotado para x), utilizando o valor de x (nproc_per_node) como 1, 2, 4 e 8. 
docker run --env OMP_NUM_THREADS=1 --rm --network=host -v=$(pwd):/root dist_dcgan:latest python -m torch.distributed.launch --nproc_per_node=x --nnodes=1 --node_rank=0 --master_addr="172.17.0.1" --master_port=1234 dist_dcgan.py --dataset cifar10 --dataroot ./cifar10 --batch_size 16 --num_epochs 1

4. Os resultados foram armazenados na tabela results_ativ_5.pdf (que está nessa pasta) e extraído os valores de análise conforme descrito no relatório. 


