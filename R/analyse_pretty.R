#!/usr/bin/Rscript

###### Libraries
library("lattice")

#CYCLES_S <- 2660046 #number of cycles for 1ms
CYCLES_S <- 2000000 #number of cycles for 1ms
cur_wd <- getwd()

###### Arguments
args <- commandArgs(trailingOnly = TRUE)
#arg 1 : file : 'stats.csv'
#arg 2 : window size (seconds) 150
#arg 3 : conf id
#arg 4 : output file
#CYCLE_S <- 2660046 #number of cycles for 1ms


file.csv <- args[1]


##### functions

readconlines <- function(conn){

    raw.l <- scan(text=readLines(conn,-1,ok=TRUE),sep=","
        ,what=list(double(),"",double(),double(),"",double(),double(),double(),double()))
#   raw.df <- data.frame(matrix(unlist(raw.l), ncol=9, byrow=F),stringsAsFactors=FALSE)
    raw.df <- as.data.frame(raw.l)
    colnames(raw.df) <- c('t','from','pid','conf_id','engine','rx','tx','filtered','dropped')
    raw.df$t <- raw.df$t/CYCLES_S/1000

    return(raw.df)
}


###### Manipulate data
con <- file(args[1],open='r',blocking=FALSE)
time <- as.numeric(args[2]) #in seconds
window <- time*CYCLES_S*1000
old.df <- readconlines(con)
guard <- time*0.08 # 8% guard

X11()


old.df.len <- dim(old.df)[1]

old.df[old.df.len,]$t-time-guard
old.df.len <- dim(old.df)[1]
x1 <- old.df[old.df.len,]$t-time-guard

old.df <- with(old.df,subset(old.df,t>(x1)))

plot.df=with(old.df,subset(old.df,conf_id==args[3]))
    plot.len=dim(plot.df)[1]
    dt=with(plot.df,diff(t))
    plot.df=with(plot.df,cbind(plot.df[2:plot.len,],
        rx.dt=diff(rx)/dt,
        tx.dt=diff(tx)/dt,
        drop.dt=diff(dropped)/dt,
        flt.dt=diff(filtered)/dt
        ))


    ymax=with(plot.df,max(rx.dt))
dev.off()
tck <- 0.25
pdf(file=args[4],useDingbats=FALSE) 
	with(plot.df,plot((rx.dt/1e6)~t,type="l",ylim=c(0,20),
                          lwd=tck,bty="l",xlim=c(0,40),
                          xlab="Temps (secondes)",ylab="Mpps"))
	legend("topright",inset=c(0,0.05), title ="Paquets",c("Rx","Tx","Drop"),
		lty=c(1,1,1),lwd=c(1,1,1),col=c("red","green","blue"),
		bty="o"
)     

	with(plot.df,lines(tx.dt/1e6~t,type="l",col="green",lwd=tck))
	with(plot.df,lines(flt.dt/1e6~t,type="l",col="red",lwd=tck))
	with(plot.df,lines(drop.dt/1e6~t,type="l",col="blue",lwd=tck))
dev.off()




