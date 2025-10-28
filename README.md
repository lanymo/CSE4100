## Project 1
- Fork를 이용하여 parents process와 child process의 concurrent한 system을 이용하여 unix shell을 구현 프로젝트
  ### Phase 1: 리눅스 쉘에서 동작하는 command를 foreground에서 수행
  - make로 컴파일한 후에, ./myshell을 입력하여 shell 실행이 가능하다.
    1. exit, quit: shell에서 탈출한다.
    2. cd /경로 : cd를 이용해 디렉터리 경로를 이동할 수 있다. .. ' '(공백), 상대경로 및 절대경로 등이 인자 값으로 들어올 수 있다.
    3. echo, vim, cat, ls, ls -al, mkdir, rmdir 등 리눅스 쉘에 존재하는 명령어를 사용할 수 있다.
  ### Phase 2: 파이프를 포함하여 여러 개의 command를 foreground에서 수행
  - 파이프라인으로 여러 개의 command를 입력받아 수행하는 기능이 추가 됐다.
 ### Phase 3: background process를 구현하고 signal handler를 구현하여 처리
 
## Proejct 2:

## Project 3:
