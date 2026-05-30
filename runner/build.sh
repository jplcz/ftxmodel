#!/bin/sh
D=gitea.kimbap.pl/docker-images/ftxmodel-ci:latest
docker build -t $D .
docker push $D
