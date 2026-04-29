pipeline {
  agent { label 'PACKAGES_x86_64_linux_redhat_7' }
  environment {
    REPOS = "core"
  }
  stages {
    stage('Clean workspace') {
      steps {
        sh 'for r in $REPOS; do rm -rf "$(basename "$r")"; done'
      }
    } // clean workspace
    stage('autoconf') {
      steps {
        sh "./autogen.sh --enable-debug"
      } // autoconf steps
    } // autoconf stage
  } // stages
} // pipeline
