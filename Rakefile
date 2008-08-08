# use this rake file to build, configure, and test nginx esi module
require 'rake'
require 'rake/testtask'
require 'yaml'

def load_config
  $config ||= YAML.load_file('.nginx_config')
end

desc 'Setup up your build environment NGINX_SRC=/path/to/nginx/src/'
task :setup do
  nginx_src = ENV['NGINX_SRC']
  if nginx_src.nil? or !File.exist?(nginx_src)
    puts "You must set NGINX_SRC enironment variable"
    exit(1)
  end
  # write configuration
  config = {
    :nginx_src => File.expand_path(nginx_src),
    :mod_src => File.expand_path(File.dirname(__FILE__))
  }
  File.open(".nginx_config","w") do|f|
    puts "Saving config: #{YAML.dump(config)}"
    f << YAML.dump(config) 
  end
end

desc 'Configure nginx'
task :configure do
  load_config
  build_string = "cd #{$config[:nginx_src]} && ./configure --prefix=#{$config[:mod_src]}/nginx/"
  if ENV['RELEASE'].nil?
    build_string << " --with-debug"
  end
  build_string << " --add-module=#{$config[:mod_src]}"
  puts build_string.inspect
  sh build_string
end

desc 'Compile nginx'
task :compile do
  load_config
  sh "cd #{$config[:nginx_src]} && make install"
end

desc 'Build and configure nginx, use RELEASE=true to build without debugging'
task :build => [:configure,:compile]

desc 'Start nginx for testing'
task :start do
  load_config
  sh "cd nginx && ./sbin/nginx -c #{File.join(File.dirname(__FILE__),'test/config/nginx-esi.conf')}"
end

desc 'Start nginx in gdb '
task :gdb do
  load_config
  sh "cd nginx && echo 'run -c #{File.join(File.dirname(__FILE__),'test/config/nginx-esi.conf')}' && gdb ./sbin/nginx"
end
desc 'Start nginx with valgrind'
task :valgrind do
  load_config
  sh "cd nginx && valgrind --leak-check=full ./sbin/nginx -c #{File.join(File.dirname(__FILE__),'test/config/nginx-esi.conf')}"
end

desc 'Stop nginx test server'
task :stop do
  load_config
  pid_file = "#{$config[:mod_src]}/nginx/logs/nginx.pid"
  if !File.exist?(pid_file)
    STDERR.puts "Expected nginx pid file: (#{pid_file}) not found!"
    exit(1)
  else
    pid = `cat #{pid_file}`
    sh "kill #{pid}"
    sh "rm -f #{pid_file}"
  end
end

namespace :ragel do
  def ragel_version
    `ragel --version`.scan(/version ([0-9\.]+)/).first.first
  end
  desc 'test the ragel version'
  task :verify do
    if ragel_version < "5.24"
      puts "You need to install a version of ragel greater or equal to 5.24"
      exit(1)
    else
      puts "Using ragel #{ragel_version}"
    end
  end

  desc 'generate the ragel parser'
  task :gen => :verify do
    if ragel_version < "6.0"
      sh "ragel ngx_esi_parser.rl | rlgen-cd -G1 -o ngx_esi_parser.c"
    else
      sh "ragel ngx_esi_parser.rl -G1 -o ngx_esi_parser.c"
    end
    raise "Failed to build ESI parser source" unless File.exist? "ngx_esi_parser.c"
  end

  desc 'generate a PNG graph of the parser'
  task :graph => :verify do
    if ragel_version < "6.0"
      sh 'ragel ngx_esi_parser.rl | rlgen-dot -p > esi.dot'
    else
      sh 'ragel -V ngx_esi_parser.rl -p > esi.dot'
    end
    sh 'dot -Tpng esi.dot -o esi.png'
  end
end

Rake::TestTask.new do |t|
  t.test_files = FileList["test/*_test.rb"]
  t.verbose = true
end

task :default => [:compile,:test]
