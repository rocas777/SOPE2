# SOPE2
Para compilar execute o commando "make".
Para correr executar "./Q1 <-t nsecs> fifoname" e "./U1 <-t nsecs> fifoname".

O programa está de acordo com as especificações que estão no enunciado.
Alguns pormenores que ficaram ao nosso critério, por exemplo o tempo inicial, é calculado logo no início do programa. 
O fecho do Serviço Q1 é comunicado através do uso da função unlink, verificando depois no U1, a usa existência , antes de escrever nesse mesmo fifo.
Para calcular o tempo de espera usamos um wrapper do nanosleep que é usado no ciclo de criação.
A sincronização é feita utilizando Mutex's e CondVariables.
Nós imprimimos o valor de retorno da função time() como diz no enunciado.
Quando o Q1 fecha o link para o fifo público, o U1 também pára de criar novas threads e acaba. No entanto, como diz no enunciado, as threads já criada são tratadas de acordo com o enunciado.

O types.h tem a struct do tipo request.
