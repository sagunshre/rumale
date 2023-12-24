#!/bin/zsh

FROM='0.28.0'
TO='0.28.1'

for FILENAME in `find . -name 'version.rb'`; do
  gsed -i -e "s/VERSION\s*=\s*'${FROM}'/VERSION = '${TO}'/" ${FILENAME}
done

for FILENAME in `find . -name '*.gemspec'`; do
  gsed -i -e "s/\(spec.add_dependency\s\+'rumale\-[a-zA-Z\-_]\+'\),\s\+'~>\s*${FROM}'/\1, '~> ${TO}'/g" ${FILENAME}
done
