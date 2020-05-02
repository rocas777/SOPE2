# SOPE2
Para compilar execute o commando "make".
Para correr executar "./Q1 <-t nsecs> fifoname" e "./U1 <-t nsecs> fifoname".

O programa está de acordo com as especificações que estão no enunciado.
Alguns pormenores que ficaram ao nosso critério por exemplo o tempo inicial é calculado logo no início do programa. Para calcular o tempo de espera usamos um wrapper do nanosleep pa dormir, e, usamos no ciclo de criação.
Nós imprimimos o valor de retorno da função time() como diz no enunciado.
Quando o Q1 fecha o link para o fifo público, o U1 também para de criar novas threads e acaba. No entanto, como diz no enunciado, as threads já criada são tratadas de acordo.
