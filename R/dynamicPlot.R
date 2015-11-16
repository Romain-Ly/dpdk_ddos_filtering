#!/usr/bin/Rscript

###### Cosntants
CYCLES_S <- 2660046 #number of cycles for 1ms




###### Libraries
library("lattice")


cur_wd <- getwd()
args <- commandArgs(trailingOnly = TRUE)
#arg 1 : file : 'stats.csv'
#arg 2 : window size (seconds) 150
#arg 3 : conf_id

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


    
##### Initialisation
con <- file(args[1],open='r',blocking=FALSE)
time <- as.numeric(args[2]) #in seconds
window <- time*CYCLES_S*1000
print(time)
guard <- time*0.08 # 8% guard

X11()



##### Script
#Sys.sleep(time)

# Read some data after sleep
old.df <- readconlines(con)
Sys.sleep(0.1)

while(1){
    
    flush.console()
    # read new data, merge and subset to window size
    new.df <- readconlines(con)

    old.df <- rbind(old.df,new.df)
    old.df.len <- dim(old.df)[1]
    
    x1 <- old.df[old.df.len,]$t-time-guard

    old.df <- with(old.df,subset(old.df,t>(x1)))
                                        #    raw.df[(raw.len-window):raw.len,]
    
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
    with(plot.df,plot(rx.dt~t,type="l",ylim=c(0,ymax)))
    with(plot.df,lines(tx.dt~t,type="l",col="green"))
    with(plot.df,lines(flt.dt~t,type="l",col="red"))
    with(plot.df,lines(drop.dt~t,type="l",col="blue"))

#    with(old.df,xyplot(rx~t|conf_id,type="l"))
    Sys.sleep(0.2)

}



close(con)
