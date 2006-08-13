#!/usr/bin/perl -w

use Term::ReadKey;

use Term::ANSIColor qw(:constants);
#$Term::ANSIColor::AUTORESET = 1;

use Getopt::Std;
local( $opt_p, $opt_P );
getopts( "p:P:" );  

$light_pattern = $opt_p || "";
$heavy_pattern = $opt_P || "\\b";




$infile = "enum.h";
open( DATA, $infile ) || die "can't open $infile: $!\n";

$category = "";


while( <DATA> )
{
  chop;

  s#/.*##;           # strip comments
  s#\*.*##;          # strip comments
  s/#.*//;
  next unless /\S/;  # skip empty lines
  next unless /\w/;  # skip special chars

  if ( m/^enum\s+(\S+)/ )
  {
    $category = $1;
    $data{ $category }{ i } = 0;
  }
  else
  {
    s/,//;   # usually a comma

    # possibly a number on the line
    ( $term, undef, $i ) = split; 
    $string = $term;

    if ( defined $i && $i =~ m/^\d+$/ )
    {
      $data{ $category }{ i } = $i;
    }
    else
    {
      $i = $data{ $category }{ i };
    }

    $data{ $category }{ array }{ $i } =  $string;
    $data{ $category }{ i }++;
  }
}


$let = "0";
foreach $cat ( sort keys %data )
{
  $letters{ $let } = $cat;
  $let++;
  $let = "A" if $let eq "10";
  $let = "a" if $let eq "AA";
}

# @sort_lets = sort { ($b ge "a") <=> ($a ge "a") || $a cmp $b } keys %letters;
@sort_lets = sort keys %letters;
foreach $let ( @sort_lets )
{
  push @key, "$let - $letters{ $let }";
}

$klen = 0;
foreach $k ( @key )
{
  $lk = length $k;
  $klen = $lk if $lk > $klen;
}
    
$klen += 1;
$klen = 20 if $klen > 20;

@bkey = @key;
foreach $k ( @bkey )
{
  if ( length $k < $klen )
  {
    $k .= " " x ($klen - length $k);
  }
  else
  {
    $k = substr( $k, 0, $klen - 1 );
    $k .= " ";
  }
}

$ntpl = int 80/$klen;
while ( @bkey )
{
  push @kd, join "", @bkey[0..$ntpl-1], "\n";
  splice @bkey, 0, $ntpl;
}

foreach $file ( @ARGV )
{
  open  IN, $file or die "can't open $file: $!\n";

  @buff = <IN>;
  $numlines = @buff;

  @save_buff = @buff;

  @lines_changed = ();

LINE:
  for( $line = 0; $line < $numlines; $line++ )
  {
    if ( $light_pattern && $buff[ $line ] !~ m/$light_pattern/io )
    {
      next LINE;
    }

    $target = $buff[ $line ];
    %poslist = ();

    while ( $target =~ m/$heavy_pattern(\d+)\b/go ) 
    {

      $number = $1;
      $pos = pos( $target ) - length $number;

      $pre = $`;

      next if $pre =~ m#//#;

      next if $pre =~ m#[\-+*/%] ?$#;

      # skip some unlikely numbers 
      # you[0]
      next if $number eq "0" && $pre =~ m/you ?\[ ?$/;
      # arithmetic
      next if $number eq "1" && $pre =~ m/\D-$/;

         
      $poslist{ $pos } = $number;
    }
      
    $longer = 0;
    foreach $pos ( sort { $a <=> $b } keys %poslist )
    {
      $number = $poslist{ $pos };

      $nl = length $number;

      print "\n";
      print YELLOW, @buff[ $line-8..$line-1];

      #print "-" x ($pos+$longer);
      #print "v" x $nl;
      #print "-" x (60 - ($nl+$pos+$longer));
      #print "\n";

      # print $target;
      print GREEN, substr( $target, 0, $pos+$longer );
      print CYAN,  substr( $target, $pos+$longer, $nl );
      print GREEN, substr( $target, $pos+$longer+$nl );


      #print "-" x ($pos+$longer);
      #print "^" x $nl;
      #print "-" x (60 - ($nl+$pos+$longer));
      #print "\n";

      print YELLOW, @buff[ $line+1..$line+8];

      print "\n";
      print RESET;
      print @kd;

      print "\n";
      print GREEN, "$file ", BLUE, "$line ", YELLOW, "replace? ";
      print RESET;

      if ( ! $do_all )
      {
        ReadMode('cbreak');
        $ans = ReadKey(0);
        ReadMode('normal');
 
        print $ans;
      }
      else
      {
        $ans = $do_all;
      }

      if ( defined $letters{ $ans } )
      {
        print "\n";

        # get busy
        $list = $letters{ $ans };
        # $rep = ${$data{$list}}[ $number ];
        $rep = $data{$list}{array}{ $number };

        if ( $rep )
        {
          substr( $target, $pos+$longer, $nl ) = $rep;
          $longer += length( $rep );
          $longer -= $nl;

          print RED, $target;
          print RESET;

          push @lines_changed, $line;
        }
        else
        {
           print BLINK, RED, "###### trouble! ######\n";
           print RESET;
        }
      }
      elsif ( $ans eq "#" )
      {
        $more = <STDIN>;
        print $more;
        chop $more;
       
        $cmd = substr $more, 0, 1;
        if ( $cmd eq "u" )
        {
          print "\n";
          if ( @lines_changed )
          {
            $line_changed = pop @lines_changed;
          
            $buff[ $line_changed ] = $save_buff[ $line_changed ];
            $line = $line_changed; 
            redo LINE;
          }
          else
          {
            redo;
          }
        }
        elsif ( $cmd eq "l" )
        {
          $newline = substr $more, 1;
          $newline += 0;
          $line = $newline if $newline;
          redo LINE;
        }
        elsif ( $cmd eq "s" )
        {
          $saving = 1;
          last LINE;
        }
        elsif ( $cmd eq "a" )
        {
          $do_all = substr $more, 1;
          print "Really answer with $do_all? ";
          $yn = <STDIN>;
          if ( $yn !~ m/^yes$/ )
          {
            $do_all = "";
          }
          redo;
        }
        elsif ( $cmd eq "q" )
        {
          exit;
        }
      }
      else
      {
        if ( $ans =~ m/\S/ )
        {
          redo;
        }
      }
    }
    $buff[ $line ] = $target;
  }
  close IN;

  if ( @lines_changed )
  {
    $bak = "$file.bak";

    if ( rename( $file, $bak ) )
    {
      $outfile = $file;
    }
    else
    {
      $outfile = "Z$file";
      print "***** printing changes to $outfile *****\n";
    }

    open OUT, "> $outfile" or die "can't open $outfile: $!\n";
    print OUT @buff;
    close OUT;

  }
  exit if $saving;
}

