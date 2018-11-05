#include "i18n-format.h"

struct ReplacePoint {
 int start;
 int end;
 char* str;
};

char* _format(char* formatS, size_t cnt, ...){
  char* subStr;
  char ch;
  struct ReplacePoint points[cnt];

  size_t maxPos = strlen(formatS);
  int start = 0;
  int end = 0;
  int cntStart = 0;
  int cntEnd = 0;
  bool isOpen = false;
  int cntFormat = 0;

  for( int i = 0; i < maxPos; i++ ){
    subStr = (char*) malloc((size_t) MAX_PARAM_LENGTH);
    subStr[0] = '\0';
    ch = formatS[i];
    if(ch == '{'){
      if(isOpen){
        printf("<858>format Error: already open {\n%s\n", formatS);
        exit(-1);
      }
      start = i;
      isOpen = true;
      cntStart++;
    }
    if(ch == '}'){
      if(!isOpen){
        printf("<859>format Error: is not opened }\n%s\n", formatS);
        exit(-2);
      }
      end = i+1;
      isOpen = false;
      cntEnd++;
      strncpy( subStr, &formatS[start], end-start+1 );
      subStr[end-start]='\0';

      points[cntFormat].start = start;
      points[cntFormat].end = end;
      points[cntFormat].str = subStr;
      cntFormat++;
    }
  }

  if(cntStart != cntEnd){
    printf("<860>format Error: {cnt != }cnt\n%s\n", formatS);
    exit(-3);
  }
  if(cntFormat != cnt){
    printf("format Error: cntFormat != cnt\n");
    exit(-4);
  }

  // about va_list
  va_list argList;

  va_start(argList, formatS);

  char* rs = (char*) malloc((size_t) MAX_FORMAT_RESULT_LENGTH);
  rs[0]='\0';

  char* params[cnt];
  for(int i = 0; i < cnt; i++){
    params[i] = va_arg(argList, char*);
  }
  va_end(argList);

  int concatStart = 0;

  struct ReplacePoint p;

  for(int i = 0; i < cnt; i++){
    p = points[i];

    if(p.start-concatStart != 0){
      char* prefix = (char*) malloc((size_t) p.start-concatStart+1);
      prefix[0]='\0';
      strncpy(prefix, &formatS[concatStart], p.start-concatStart);
      prefix[p.start-concatStart] = '\0';
      strcat( rs, prefix );
    }

    int numNumber = strlen(p.str)-2;
    char* onlyNumber = (char*) malloc((size_t) numNumber+1);
    strncpy( onlyNumber, &(p.str)[1], numNumber );
    onlyNumber[numNumber] = '\0';

    strcat( rs, params[ atoi(onlyNumber)] );
    
    concatStart=p.end;
    if(i == cnt-1){
      char* tail = (char*) malloc((size_t) strlen(formatS)-concatStart+1);
      tail[0]='\0';
      strncpy(tail, &formatS[concatStart],  strlen(formatS)-concatStart);
	  tail[strlen(formatS)-concatStart] = '\0';
      strcat(rs, tail);
    }
  }

  return rs;
}

/*
int main(void){
  printf( format("{01 : attacker }(은)는 {00 : monster }를 { 2 : to do }했다.\n", "코볼트", "당신", "공격") ); // kobold, you, attack
  printf( format("{01 : attacker } {2 : todo }ed { 0 : to do }. \n", "kobold", "you", "attack") );

  return 0;
}
*/

