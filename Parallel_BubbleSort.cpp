#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define SEED 5
#define N 200000
int *array;    // Arreglo original
int *localArray; // Arreglo de proceso
int	taskid,    // ID del procesador */
	numtasks,    // Numero de procesadores a usar
  start,       // Index de incio de su parte del arreglo
  size,        // Tamaño de elementos del procesador
  left=-1,     // Indice del vecino a la izquierda
  right=-1,    // Indice del vecino a la derecha
	*result;
void createFile(char c[]);
void loadFile(char c[]);
void splitArray();
void sendAndSort();
void collectFromWorkers();
void reportToMaster();

int main(int argc, char *argv[]) {

  char c[]="Array1.txt";
  //Crea el arreglo a ordenar
  createFile(c);

  /* Inicializa MPI */
	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD,&taskid);
	MPI_Comm_size(MPI_COMM_WORLD,&numtasks);

  //Carga el archivo
  loadFile(c);

  //Asigna los vecinos de cada proceso
  if(taskid==0){
    right=taskid+1;
  }else if(taskid==numtasks-1){
    left=taskid-1;
  }else{
    right=taskid+1;
    left=taskid-1;
  }

  //Imprime el arreglo original
  if(taskid==0){
    printf("Arreglo Original\n");
    for(int i=0;i<N;i++)
      printf("%d ",array[i]);
    printf("\n");
  }
	//Comienza a medir el Tiempo
	double ti= MPI_Wtime();
  //Divide el arreglo a cada proceso
  splitArray();

  //Cada proceso ordena su parte y se comunica
  sendAndSort();

  if(taskid==0){
    //Recolecta los resultados de los trabajadores
    collectFromWorkers();
  }else{
    //Envia los resultados al nodo maestro
    reportToMaster();
  }
	//Termina de medir el tiempo
	double tf= MPI_Wtime();
	if(taskid==0){
		//Imprime el arreglo final ordenado
	  printf("*****************************\n");
	  printf("Arreglo Ordenado\n");
	  for(int i=0;i<N;i++)
	    printf("%d ",result[i]);
	  printf("\n");
		//Imprime el tiempo de ejecucion
		printf("Tiempo de ejecucion: %f\n",(tf-ti) );
		free(result);
	}
  //Finaliza la seccion paralela
  MPI_Finalize();
  free(array);
  free(localArray);
  return 0;
}

void createFile(char c[]){
	//Crea un puntero tipo file
	FILE *file;
	file=fopen(c,"w");
	if( file==NULL )
		printf("Error to create File\n");
	else
	{
		//Escribe una linea en el archivo
		fprintf(file,"%d\n",N);
		//Define la semilla para generar numeros aleatorios
		srand(SEED);
		//Escribe los n numeros en el archivo
		for(int i=0;i<N;i++){
			fprintf(file,"%d\n",rand()%100);
		}
	}
	fclose(file);
}
void loadFile(char c[]){
	//Creamos un puntero de tipo FILE
	FILE *fp;
	//Abrimos el archivo a leer
 	if((fp = fopen (c,"r" ))==NULL){
 		printf("No se pudo leer el archivo\n");
 	}else{
 		int n=0;
 		fscanf(fp,"%d",&n);
 		array=(int *)malloc(n*sizeof(int));
 		//Cargamos los elementos del arreglo
 		for (int i = 0; i < n; ++i)
 			fscanf(fp,"%d",&array[i]);
 	}
}
void splitArray(){
  int nmin, nleft, nnum;
  //Define el tamaño de particion
  nmin=N/numtasks;
  nleft=N%numtasks;
  int k=0;
  for (int i = 0; i < numtasks; i++) {
     nnum = (i < nleft) ? nmin + 1 : nmin;
     if(i==taskid){
       start=k+1;
       size=nnum;
       printf ("tarea=%2d  Inicio=%2d  tamaño=%2d  right=%2d  left=%2d\n", taskid,start, size, right, left);
       localArray=(int *)malloc((size+2)*sizeof(int));
       //Copia los elemntos del arreglo original al local
       for(int j=1;j<size+1;j++)
        localArray[j]=array[k+j-1];
     }
  k+=nnum;
  }
}
void sendAndSort(){
  MPI_Status status;
  for(int i=0;i<N;i++){
    int sleft=0,sright=0;
    //Corrida par
    if(i%2==0){
      for(int j=0;j<numtasks;j++){
        if(j==taskid){
            int flag=(start+size-1)%2;
            if(flag==1 && taskid < numtasks-1){
              sright++;
              //Enviar ultimo elemnto a la derecha
              //printf("Proceso %d envia a %d\n",taskid,right);
              MPI_Send(&localArray[size] //referencia al vector de elementos a enviar
                      ,1 // tamaño del vector a enviar
                      ,MPI_INT // Tipo de dato que envias
                      ,right // pid del proceso destino
                      ,0 //etiqueta
                      ,MPI_COMM_WORLD); //Comunicador por el que se manda
              //Recibir de la tarea a la derecha
              //printf("Proceso %d recibe de %d\n",taskid,right);
              MPI_Recv(&localArray[size+1] // Referencia al vector donde se almacenara lo recibido
                      ,1 // tamaño del vector a recibir
                      ,MPI_INT // Tipo de dato que recibe
                      ,right // pid del proceso origen de la que se recibe
                      ,1 // etiqueta
                      ,MPI_COMM_WORLD // Comunicador por el que se recibe
                      ,&status); // estructura informativa del estado
            }/*else*/ if((start%2)==0 && taskid > 0){
              sleft++;
              //Enviar primer elemnto a la Izquierda
              //printf("Proceso %d envia a %d\n",taskid,left);
              MPI_Send(&localArray[1],1,MPI_INT,left,1,MPI_COMM_WORLD);
              //Recibir ultimo elmento de la izquierda
              //printf("Proceso %d recibe de %d\n",taskid,left);
              MPI_Recv(&localArray[0],1,MPI_INT,left,0,MPI_COMM_WORLD,&status);
            }
        }
      }
      //Ordenamiento por corrida par
      for(int k=1-sleft;k<size+sright;k+=2)
        if(localArray[k]>localArray[k+1]){
          //Intercambio binario
          localArray[k]^=localArray[k+1];
          localArray[k+1]^=localArray[k];
          localArray[k]^=localArray[k+1];
        }
    }
    //Corrida non
    else{
      for(int j=0;j<numtasks;j++){
        if(j==taskid){
            int flag=(start+size-1)%2;
            if((start%2)==1 && taskid > 0){
              sleft++;
              //Enviar ultimo elemnto a la Izquierda
              //printf("Proceso %d envia a %d\n",taskid,left);
              MPI_Send(&localArray[1],1,MPI_INT,left,1,MPI_COMM_WORLD);
              //Recibir ultimo elmento de la izquierda
              //printf("Proceso %d recibe de %d\n",taskid,left);
              MPI_Recv(&localArray[0],1,MPI_INT,left,0,MPI_COMM_WORLD,&status);

            }/*else*/ if(flag==0 && taskid < numtasks-1){
              sright++;
              //Enviar ultimo elemnto a la derecha
              //printf("Proceso %d envia a %d\n",taskid,right);
              MPI_Send(&localArray[size],1,MPI_INT,right,0,MPI_COMM_WORLD);
              //Recibir de la tarea a la derecha
              //printf("Proceso %d recibe de %d\n",taskid,right);
              MPI_Recv(&localArray[size+1],1,MPI_INT,right,1,MPI_COMM_WORLD,&status);
            }
        }
      }
      //Ordenamiento por corrida non
      for(int k=(start%2)+1-(sleft*2);k<size+sright;k+=2)
        if(localArray[k]>localArray[k+1]){
          //Intercambio binario
          localArray[k]^=localArray[k+1];
          localArray[k+1]^=localArray[k];
          localArray[k]^=localArray[k+1];
        }
    }
  }
  /*for(int i=1;i<size+1;i++)
    printf("[%d] %d ",taskid,localArray[i]);
  printf("\n");*/
}
void collectFromWorkers(){
  MPI_Status status;
  result=	(int *)malloc(N*sizeof(int));
  int buffer[2];
  for(int i=1;i<numtasks;i++){
    //Recibe parametros de los trabajadores
    MPI_Recv(buffer,2,MPI_INT,i,0,MPI_COMM_WORLD,&status);
    int iStart=buffer[0];
    int iSize=buffer[1];
    //Recibe la parte del arreglo de cada trabajador
    MPI_Recv(&result[iStart-1],iSize,MPI_INT,i,1,MPI_COMM_WORLD,&status);
  }
  //Añade los resultados del nodo maestro al resultados
  for(int i=start;i<start+size;i++)
    result[i-1]=localArray[i];
}
void reportToMaster(){
  int buffer[2];
  buffer[0]=start;
  buffer[1]=size;
  //Envia los parametros al proceso Maestro
  MPI_Send(buffer,2,MPI_INT,0,0,MPI_COMM_WORLD);
  //Envia su parte del arrgelo al proceso Maestro
  MPI_Send(&localArray[1],size,MPI_INT,0,1,MPI_COMM_WORLD);
}
